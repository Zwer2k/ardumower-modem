#include "terminal.h"

#ifdef MOWER_TERMINAL
using namespace ArduMower::Modem;

void TerminalMessage::marshal(JsonObject o) const
{
  JsonArray logJson = o["lines"].to<JsonArray>();
  JsonObject jsonLine = logJson.add<JsonObject>();
  jsonLine["nr"] = 0;
  jsonLine["isSend"] = false;
  jsonLine["text"] = message;
}

Terminal::Terminal(Stream &serial)
{
  _terminalRouter = new Router(serial);
  _terminalRouter->sniffRx(this);
  _terminalRouter->sniffTx(this);
  _buffer = new Ringbuffer<String, TERMINAL_RINGBUFFER_SIZE>();
}

Terminal::~Terminal() {
  delete _terminalRouter;
  delete _buffer;
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
  _buffer->push(&line, true);
  if (_rxHandler != NULL) {
    _rxHandler(line);
  }
}

void Terminal::drainTx(String line, bool &stop)
{
  _buffer->push(&line, true);
  if (_txHandler != NULL) {
    _txHandler(line);
  }
}

uint16_t Terminal::marshalBatch(const JsonObject &o, uint16_t startIdx, uint16_t maxLines)
{
  uint16_t count = _buffer->currentSize();
  if (startIdx >= count) return 0;
  JsonArray logJson = o["lines"].to<JsonArray>();
  String line;
  uint16_t sent = 0;
  for (uint16_t i = startIdx; i < count && sent < maxLines; i++) {
    if (_buffer->peekAt(i, line)) {
      JsonObject jsonLine = logJson.add<JsonObject>();
      jsonLine["nr"] = 0;
      jsonLine["isSend"] = false;
      jsonLine["text"] = line;
      sent++;
    }
  }
  return sent;
}

uint16_t Terminal::bufferSize()
{
  return _buffer->currentSize();
}
#endif