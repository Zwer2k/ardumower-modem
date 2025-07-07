#pragma once

#include <Arduino.h>
#include "json.h"
#include "router.h"

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
    };
  }
}