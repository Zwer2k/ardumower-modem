#pragma once

#ifdef MOWER_TERMINAL
#include "terminal.h"
#endif
#include "stm32ota/stm32ota.h"
#include <list>
#include <FS.h>
#include <ArduinoJson.h>

namespace ArduMower
{
  namespace Modem
  {
    namespace Ota
    {
      class StatusMessage
      {
      public:
        uint32_t timestamp;
        byte progress;

        StatusMessage(byte progress) : timestamp(millis()), progress(progress){};
  void marshal(const ArduinoJson::JsonObject &o) const;
      };

      using UpdateComplete = std::function<void(String updateResult)>;
      using StatusHandler = std::function<void(byte progress)>;
      using IdleCallback = std::function<void(void)>;

      class MowerUpdater
      {
      public:
  #ifdef MOWER_TERMINAL
  MowerUpdater(Terminal &terminal, HardwareSerial &mowerFirmwareSerial);
  #else
  MowerUpdater(HardwareSerial &mowerFirmwareSerial);
  #endif

        void startUpdate(String filename, UpdateComplete updateComplete);
        String handleFlash();
        void addStatusHandler(StatusHandler handler);
        void addIdleCallback(IdleCallback cb);
        void loop();
        static bool isFlashing() { return _isFlashing; }

      private:
        #ifdef MOWER_TERMINAL
        Terminal &_terminal;
        #endif
        HardwareSerial &_serial;
        String _filename;
        FirmwareWriterSTM32 firmwareWriter;
        File fsUploadFile;

        bool _serialPortReady = false;
        bool _fileUploaded = false;
        UpdateComplete _updateComplete;
        StatusHandler _statusHandler;
        std::list<IdleCallback> _idleCallbacks;

        byte _progress = 255;
        static bool _isFlashing;

        void runIdleCallbacks();
        void updateStatus(byte progress);
        void setSerialPortReady(bool serialPortReady);
        void printBootloaderInfo();
  #ifdef MOWER_TERMINAL
  void resumeTerminal() { _terminal.resume(); }
  #endif
      };
    }
  }
}
