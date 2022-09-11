
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
      enum RequestDataType {
        requestHello = 0,
        modemLogSettings,
        requestDataTypeLength
      };

      enum ResponseDataType {
        responseHello = 0,
        mowerState,
        mowerStats,
        desiredState,
        modemLog,
        responseDataTypeLength
      };

      class UiSocketHandler;

      class UiSocketItem
      {
      public:
        UiSocketItem(
          UiSocketHandler *socketHandler,
          AsyncWebSocketClient *client, 
          ArduMower::Domain::Robot::StateSource &source);
        void handleData(RequestDataType dataType, DynamicJsonDocument &jsonData);
        void sendText(String text);
        void pingClients();

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
          AsyncWebSocket *ws,
          ArduMower::Domain::Robot::StateSource &source,
          ArduMower::Domain::Robot::CommandExecutor &cmd);
        
        void loop();
        void sendData(ResponseDataType dataType, UiSocketItem *sendTo = NULL, bool force = false);
        void logToUiLoop();
        void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

        ~UiSocketHandler();
      
      private:
        AsyncWebSocket *_ws;

        uint32_t oldDataTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastDataRequestTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastclientPing = 0;
        
        ArduMower::Domain::Robot::StateSource &_source;
        ArduMower::Domain::Robot::CommandExecutor &_cmd;

        void handleData(uint32_t clientId, char *data);
        std::map<uint32_t, UiSocketItem*> itemMap;  

        void stateRequestLoop();
        template<typename T>
        void sendData(UiSocketItem *sendTo, ResponseDataType dataType, T data, bool force = false);
        void pingClients();
      };
    }
  }
}