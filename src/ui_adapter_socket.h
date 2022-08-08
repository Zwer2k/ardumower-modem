
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
        hello = 0,
        mowerState,
        mowerStats,
      };

      class UiSocketHandler;

      class UiSocketItem
      {
      public:
        UiSocketItem(
          UiSocketHandler *socketHandler,
          AsyncWebSocketClient *client, 
          ArduMower::Domain::Robot::StateSource &source);
        void handleData(DataType dataType, DynamicJsonDocument &jsonData);
        void sendText(String text);

        ~UiSocketItem();
      
      private:
        UiSocketHandler *_socketHandler;
        AsyncWebSocketClient *_client;
        ArduMower::Domain::Robot::StateSource &_source;
      };      

      class UiSocketHandler
      {
      public:
        UiSocketHandler(
          ArduMower::Domain::Robot::StateSource &source,
          ArduMower::Domain::Robot::CommandExecutor &cmd);
        
        void loop();
        void sendState(UiSocketItem *sendTo = NULL);
        void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

        ~UiSocketHandler();
      
      private:
        uint32_t oldStateTimestamp = 0;
        uint32_t newDataRequestTimestamp = 0;

        ArduMower::Domain::Robot::StateSource &_source;
        ArduMower::Domain::Robot::CommandExecutor &_cmd;

        void handleData(uint32_t clientId, char *data);
        std::map<uint32_t, UiSocketItem*> itemMap;  
      };
    }
  }
}