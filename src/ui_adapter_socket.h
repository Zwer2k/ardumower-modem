
#pragma once

#include "log.h"
#include <ESPAsyncWebServer.h>
#include <map>
#include <ArduinoJson.h>
#include "domain.h"

namespace ArduMower
{
  namespace Modem
  {
    namespace Http
    {
      enum DataType {
        mowerState = 1,
        mowerStats,
      };

      class UiSocketItem
      {
      private:
        AsyncWebSocketClient *_client;
        ArduMower::Domain::Robot::StateSource &_source;
        
      public:
        UiSocketItem(AsyncWebSocketClient *client, ArduMower::Domain::Robot::StateSource &source);
        void handleData(DataType dataType, DynamicJsonDocument &jsonData);
        void sendState();

        ~UiSocketItem();
      };
      

      class UiSocketHandler
      {
      private:
        ArduMower::Domain::Robot::StateSource &_source;

        void handleData(uint32_t clientId, char *data);
        std::map<uint32_t, UiSocketItem*> itemMap;

      public:
        UiSocketHandler(ArduMower::Domain::Robot::StateSource &source);
        void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

        ~UiSocketHandler();
      };
    }
  }
}