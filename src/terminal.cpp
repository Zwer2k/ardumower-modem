#include "terminal.h"

using namespace ArduMower::Modem;

void TerminalMessage::marshal(const JsonObject &o) const
{
  JsonArray logJson = o.createNestedArray("lines");
  JsonObject jsonLine = logJson.createNestedObject();
  jsonLine["nr"] = 0;
  jsonLine["isSend"] = false;
  jsonLine["text"] = message;
}

Terminal::Terminal(Stream &serial)
{
  _terminalRouter = new Router(serial);
  _terminalRouter->sniffRx(this);
  _terminalRouter->sniffTx(this);
}

Terminal::~Terminal() {
  delete _terminalRouter;
}

void Terminal::begin()
{
  _terminalRouter->begin();
}

void Terminal::loop()
{
  if (suspended && !prepareSuspended) {
    return;
  }

  _terminalRouter->loop();

  if (prepareSuspended && !_terminalRouter->inAction()) 
  {
    prepareSuspended = false;
    suspended = true;
    _suspendDone();
  }
}

bool Terminal::sendWithoutResponse(String line) 
{
  if (prepareSuspended || suspended) {
    return false;
  }

  return _terminalRouter->sendWithoutResponse(line);
}

void Terminal::addRxHandler(RxHandler handler) 
{
  _rxHandler = handler;
}

void Terminal::addTxHandler(TxHandler handler) 
{
  _txHandler = handler;
}

void Terminal::suspend(SuspendDone suspendDone)
{
  prepareSuspended = true;
  _suspendDone = suspendDone;
}

void Terminal::resume()
{
  suspended = false;
  prepareSuspended = false;
}

void Terminal::drainRx(String line, bool &stop)
{
  if (_rxHandler != NULL) {
    _rxHandler(line);
  }
}

void Terminal::drainTx(String line, bool &stop)
{
  if (_txHandler != NULL) {
    _txHandler(line);
  }
}
