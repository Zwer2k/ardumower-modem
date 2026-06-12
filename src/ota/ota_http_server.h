#pragma once

#include <ESPAsyncWebServer.h>
#include "http_common.h"
#include "ota.h"
#include "settings.h"
#include "ota_mower_updater.h"

namespace ArduMower
{
  namespace Modem
  {
    namespace Ota
    {
      class HttpServer;

      extern volatile size_t otaFlashProgress;
      extern volatile size_t otaFlashTotal;
      extern volatile bool otaFlashForceSend;

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
          FLASH_FILE,
          INCOMPLETE,
          ERROR,
          INDEX_MISMATCH,
          UPDATE_BEGIN_FAILED,
          VERIFY_HEADER_FAILED,
          SHORT_WRITE_ERROR,
          UPDATE_END_FAILED,
          FLASH_PENDING,
          FLASHING,
        };

        class UploadSession
        {
        public: 
          UploadSession() {}
          virtual ~UploadSession() {}

          virtual void handle(size_t index, uint8_t *data, size_t len, bool final);
          virtual void respond(AsyncWebServerRequest *request);
        };

        class ModemUploadSession : UploadSession
        {
        private:
          static const size_t MAX_OTA_SIZE = 0x200000; // 2MB PSRAM buffer
          static const size_t FLASH_CHUNK_SIZE = 4096;

          HttpServer *s;
          Result result;
          uint8_t *_buffer;     // PSRAM buffer (NULL = streaming fallback)
          size_t _bufferPos;    // bytes buffered so far / total firmware size
          bool _streaming;      // true = direct Update.write(), no buffering

          size_t _flashWritten;
          uint8_t *_dramBuf;

          bool verifyHeader(uint8_t *data, size_t len);

        public:
          ModemUploadSession(HttpServer *_s);
          ~ModemUploadSession();

          void handle(size_t index, uint8_t *data, size_t len, bool final);
          void respond(AsyncWebServerRequest *request);

          bool isFlashPending() { return result == Result::FLASH_PENDING; }
          bool beginFlash();
          bool loopFlashWrite();
          bool endFlash();

          size_t flashProgress() { return _flashWritten; }
          size_t flashTotal() { return _bufferPos; }
        };

        class MowerUploadSession : UploadSession
        {
        private:
          HttpServer *_server;
          String _filename;
          MowerUpdater &_mowerUpdater;
          File fsUploadFile;
          Result result;
          size_t _index;
          
          bool verifyHeader(uint8_t *data, size_t len);
          String handleFlash();
          void handleListFiles();

        public:
          MowerUploadSession(HttpServer *_s, String filename, MowerUpdater &mowerUpdater);

          void handle(size_t index, uint8_t *data, size_t len, bool final);
          void respond(AsyncWebServerRequest *request);
        };
      }

      class HttpServer : public Ota, public ArduMower::Modem::Http::Common
      {
      private:
        AsyncWebServer &_server;
        MowerUpdater &_mowerUpdater;
        bool _active;
        bool _failed;
        bool _restart;
        uint32_t _restartTime;
        Http::ModemUploadSession *_flashSession;

        void handleUploadRequest(AsyncWebServerRequest *request);
        void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

        void handlePostRequest(AsyncWebServerRequest *request);
        void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

        void beginModemUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final);
        void beginMowerUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
        void continueUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final);

        void loopRestart();
        void loopFlash();
        FirmwareUploadType getUploadType(AsyncWebServerRequest *request);

      public:
        HttpServer(Settings::Settings &settings, AsyncWebServer &server, MowerUpdater &mowerUpdater);

        virtual void begin() override;
        virtual void loop() override;
        virtual bool active() override { return _active; };

        void requestRestart();
        void queueFlash(Http::ModemUploadSession *session);
        std::function<void(size_t, size_t)> onFlashProgress;
      };
    }
  }
}