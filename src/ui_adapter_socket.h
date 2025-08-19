#pragma once

#include "log.h"
#include <ESPAsyncWebServer.h>
#include <map>
#include <ArduinoJson.h>
#include "domain.h"
#ifdef MOWER_TERMINAL
#include "terminal.h"
#endif
#include "ota_mower_updater.h"

namespace ArduMower
{
  namespace Modem
  {
    namespace Http
    {
      enum RequestDataType {
        requestHello = 0,
        modemLogSettings,
        mowerConsoleRequest,
        requestDataTypeLength
      };

      enum ResponseDataType {
        responseHello = 0,
        mowerState,
        mowerStats,
        desiredState,
        modemLog,
        mowerConsole,
        map,
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
        bool sendText(String text);
        void ping();
        AwsClientStatus status();

        ~UiSocketItem();
      
      private:
        UiSocketHandler *_socketHandler;
        AsyncWebSocketClient *_client;
        ArduMower::Domain::Robot::StateSource &_source;
      };      

      class UiSocketHandler
      {
      public:
#ifdef MOWER_TERMINAL
        UiSocketHandler(
          Terminal &terminal,
          AsyncWebServer &server,
          ArduMower::Domain::Robot::StateSource &source,
          ArduMower::Domain::Robot::CommandExecutor &cmd,
          Ota::MowerUpdater &mowerUpdater
        );
#else
        UiSocketHandler(
          AsyncWebServer &server,
          ArduMower::Domain::Robot::StateSource &source,
          ArduMower::Domain::Robot::CommandExecutor &cmd,
          Ota::MowerUpdater &mowerUpdater
        );
#endif

        ~UiSocketHandler();
        
        void begin();
        void loop();
        void logToUiLoop();
        bool cmdToMower(String cmd);
        void sendData(ResponseDataType dataType, UiSocketItem *sendTo = NULL, bool force = false);
        void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

      private:
        void sendMapInChunks(UiSocketItem *sendTo, bool force);
        AsyncWebSocket *_ws;

        uint32_t oldDataTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastDataRequestTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastclientPing = 0;
        
  #ifdef MOWER_TERMINAL
  Terminal &_terminal;
  #endif
        AsyncWebServer &_server;
        ArduMower::Domain::Robot::StateSource &_source;
        ArduMower::Domain::Robot::CommandExecutor &_cmd;
        Ota::MowerUpdater &_mowerUpdater;

        void handleData(uint32_t clientId, char *data);
        std::map<uint32_t, UiSocketItem*> itemMap;  

        void stateRequestLoop();
        template<typename T>
        void sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force = false);
        void pingClients();
#ifdef MOWER_TERMINAL
        void sendTerminalLine(String line);
#endif
        void uploadStatusHandler(byte progress); 
      };
    }
  }
}