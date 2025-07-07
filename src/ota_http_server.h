#pragma once

#include <ESPAsyncWebServer.h>
#include "http_common.h"
#include "ota.h"
#include "settings.h"
#include "terminal.h"
#include "stm32ota/stm32ota.h"

namespace ArduMower
{
  namespace Modem
  {
    namespace Ota
    {
      class HttpServer;

      enum FirmwareUploadType {
        modem = 0,
        mower
      };

      namespace Http
      {
        enum Result
        {
          SUCCESS = 0,
          PENDING,
          STARTED,
          INCOMPLETE,
          ERROR,
          INDEX_MISMATCH,
          UPDATE_BEGIN_FAILED,
          VERIFY_HEADER_FAILED,
          SHORT_WRITE_ERROR,
          UPDATE_END_FAILED,
        };

        class UploadSession
        {
        public: 
          UploadSession() {}

          virtual void handle(size_t index, uint8_t *data, size_t len, bool final);
          virtual void respond(AsyncWebServerRequest *request);
        };

        class ModemUploadSession : UploadSession
        {
        private:
          HttpServer *s;
          Result result;
          size_t _index;

          bool verifyHeader(uint8_t *data, size_t len);

        public:
        ModemUploadSession(HttpServer *_s) : s(_s), result(Result::PENDING), _index(0) {}

          void handle(size_t index, uint8_t *data, size_t len, bool final);
          void respond(AsyncWebServerRequest *request);
        };

        class MowerUploadSession : UploadSession
        {
        private:
          HttpServer *_server;
          String _filename;
          File fsUploadFile;
          Result result;
          size_t _index;
          HardwareSerial _serial;
          FirmwareWriterSTM32 firmwareWriter;

          bool _serialPortReady = false;
          bool _fileUploaded = false;

          bool verifyHeader(uint8_t *data, size_t len);
          bool updateFirmware();
          String handleFlash();
          void handleListFiles();

        public:
          MowerUploadSession(HttpServer *_s, String filename, HardwareSerial &serial);

          void handle(size_t index, uint8_t *data, size_t len, bool final);
          void respond(AsyncWebServerRequest *request);

          void setSerialPortReady(bool serialPortReady);
        };
      }

      class HttpServer : public Ota, public ArduMower::Modem::Http::Common
      {
      private:
        AsyncWebServer &_server;
        Terminal &_terminal;
        HardwareSerial &_mowerFirmwareSerial;
        bool _active;
        bool _failed;
        bool _restart;
        uint32_t _restartTime;

        void handleUploadRequest(AsyncWebServerRequest *request);
        void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

        void handlePostRequest(AsyncWebServerRequest *request);
        void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

        void beginModemUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final);
        void beginMowerUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
        void continueUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final);

        void loopRestart();
        FirmwareUploadType getUploadType(AsyncWebServerRequest *request);

      public:
        HttpServer(Settings::Settings &settings, AsyncWebServer &server, Terminal &terminal, HardwareSerial &mowerFirmwareSerial);

        virtual void begin() override;
        virtual void loop() override;
        virtual bool active() override { return _active; };

        void requestRestart();
        void resumeTerminal() { _terminal.resume(); }
      };
    }
  }
}