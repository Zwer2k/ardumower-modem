
#pragma once

#include "log.h"
#include <ESPAsyncWebServer.h>

namespace ArduMower
{
  namespace Modem
  {
    namespace Http
    {

        class SocketHandler
        {
        private:

        public:
            SocketHandler();
            void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

            ~SocketHandler();
        };
    }
  }
}