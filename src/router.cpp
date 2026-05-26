#include "router.h"
#include "log.h"

using namespace ArduMower::Modem;

// expect an answer from ArduMower within expectDownResponseTimeout ms or respond to caller with "Error::TIMEOUT"
static const uint32_t expectDownResponseTimeout = 1000;

void Router::begin()
{
  while (down.available())
    down.read();
}

void Router::loop()
{
  loopSend();
  loopReceive();
  loopTimeout();
  loopStuckRecovery();
}

void Router::loopStuckRecovery()
{
  // Auch wenn sendCommand true ist (sendWithoutResponse hängt),
  // müssen wir den Router zurücksetzen
  if (!expectResponse && !sendCommand) return;

  // expectResponseSince wird genau einmal pro Befehl gesetzt und nicht
  // durch neue Befehle oder eingehende Zeichen überschrieben.
  if (_millis() - expectResponseSince > 3000) {
    Log(ERR, "Router::loopStuckRecovery: stuck for %lu ms, resetting cmd=%s", (unsigned long)(_millis() - expectResponseSince), lastCommand.c_str());
    sendCommand = false;
    expectResponse = false;
    expectResponseSince = 0;
    cb("", XferError::TIMEOUT);
  }
}

void Router::sniffRx(RxDrain *d)
{
  drains.push_back(d);
}

void Router::sniffTx(TxDrain *d)
{
  txDrains.push_back(d);
}

bool Router::send(String _command, responseCb _cb)
{
  // reject until idle
  if (sendCommand || expectResponse)
    return false;

  lastCommand = _command;
  command = _command + "\r\n";
  cb = _cb;
  sendCommand = true;
  expectResponse = true;
  lastTx = _millis();
  expectResponseSince = _millis();

  return true;
}

bool Router::sendWithoutResponse(String line)
{
  if (sendCommand || expectResponse)
    return false;

  lastCommand = line;
  command = line + "\r\n";
  cb = [](String r, int err) {};
  sendCommand = true;

  return true;
}

bool Router::inAction()
{
  return (sendCommand || expectResponse);
}

void Router::loopSend()
{
  if (!sendCommand) return;

  {
    String hex;
    for (size_t i = 0; i < command.length(); i++) {
      if (i > 0) hex += " ";
      char buf[3];
      snprintf(buf, sizeof(buf), "%02X", (unsigned char)command[i]);
      hex += buf;
    }
    Log(COMM, "SERIAL>> hex (%d): %s", command.length(), hex.c_str());
  }

  auto sent = down.print(command.c_str());
  down.flush();

  if (sent < command.length())
  {
    command = command.substring(sent);
    return;
  }

  sendCommand = false;

  bool stop = false;
  for (auto it : txDrains)
  {
    it->drainTx(lastCommand, stop);
    if (stop)
      break;
  }
}

void Router::loopReceive()
{
  while (down.available())
  {
    int c = down.read();
    if (c == -1)
      continue;
    
    lastRx = _millis();

    String line = downRx.update((char)c);

    if (line == "")
      continue;

    // Nach Zeilenabschluss: Zusammenfassung ignorierter Zeichen loggen
    String badChars = downRx.getAndClearBadChars();
    if ((!down.available()) && (badChars.length() > 0)) {
      Log(ERR, "Reader::update::bad-character-ignored: %s", badChars.c_str());
    }

    if (expectResponse)
    {
      expectResponse = false;
      expectResponseSince = 0;
      cb(line, XferError::SUCCESS);
    }

    if (!drains.empty())
    {
      bool stop = false;
      for (auto it : drains)
      {
        it->drainRx(line, stop);
        if (stop)
          break;
      }
    }

    return;
  }
}

void Router::loopTimeout()
{
  if (isRxTimeout())
  {
    expectResponse = false;
    expectResponseSince = 0;
    cb("", XferError::TIMEOUT);
  }
}

bool Router::isRxTimeout()
{
  if (!expectResponse) return false;

  // either time of "send to downstream"
  // or time of last "receive from downstream"
  const uint32_t limit = lastTx > lastRx ? lastTx : lastRx;

  return _millis() >= limit + expectDownResponseTimeout;
}
