#pragma once

#ifdef MOWER_TERMINAL

#include <Arduino.h>
#include "json.h"
#include "router.h"
#include "ringbuffer.h"

#define TERMINAL_RINGBUFFER_SIZE 200

namespace ArduMower
{
  namespace Modem
  {
    class TerminalMessage
    {
    public:
      uint32_t timestamp;
      String message;

      TerminalMessage(String message) : timestamp(millis()), message(message){};
      void marshal(const JsonObject &o) const;
    };

    using RxHandler = std::function<void(String line)>;
    using TxHandler = std::function<void(String line)>;
    using SuspendDone = std::function<void()>;

    class Terminal: public RxDrain, public TxDrain
    {
    public:
      Terminal(
        Stream &serial
      );

      ~Terminal();

      void begin();
      void loop();
      bool sendWithoutResponse(String line);
      void addRxHandler(RxHandler handler);
      void addTxHandler(TxHandler handler);
      uint16_t marshalBatch(const JsonObject &o, uint16_t startIdx, uint16_t maxLines);
      uint16_t bufferSize();

      void suspend(SuspendDone suspendDone);
      void resume();
      bool isSuspended() { return suspended; }
      
      virtual void drainRx(String line, bool &stop) override;
      virtual void drainTx(String line, bool &stop) override;

    private:
      ArduMower::Modem::Router *_terminalRouter;
      RxHandler _rxHandler;
      TxHandler _txHandler;
      SuspendDone _suspendDone;

      bool suspended = false;
      bool prepareSuspended = false;
      Ringbuffer<String, TERMINAL_RINGBUFFER_SIZE> *_buffer;
    };
  }
}
#endif