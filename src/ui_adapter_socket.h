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
#include <vector>

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
        requestGpsDetails,
        stopGpsDetails,
        requestSensorSummary,
        stopSensorSummary,
        requestUbx,
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
        sensorSummary,
        gpsDetails,
        ubxResponse,
        responseDataTypeLength
      };

      enum class MapPointType {
        Perimeter,
        Exclusion,
        Dockpoints,
        Waypoints
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

      struct MapChunkSendState {
        bool active = false;
        UiSocketItem* sendTo = nullptr;
        uint32_t timestamp = 0;
        int phase = 0;
        size_t exclusionIdx = 0;
        size_t idx = 0;
        ArduMower::Domain::Robot::MowerMap snapshot;  // Einmalige Kopie der Map beim Start
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
        bool sendUbx(const String &hexCmd);
        void resetRequestTimestamp(ResponseDataType dataType);

        bool gpsDetailsActive = false;
        bool sensorSummaryActive = false;
        bool ubxResponseActive = false;
        String pendingUbxCmd;

      private:
        void startMapChunkSend(UiSocketItem* sendTo, bool force);
        void processMapChunkSend();
        bool sendMapChunk(MapPointType pointType, const std::vector<ArduMower::Domain::Robot::MapPoint>& points, uint32_t timestamp, UiSocketItem* sendTo, int exclusionIdx, size_t startIdx, size_t blockSize);
        AsyncWebSocket *_ws;

        uint32_t lastVersionRequestTimestamp = 0;
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
        MapChunkSendState mapChunkSendState;
        
        void versionRequestLoop();
        void stateRequestLoop();
        void sensorRequestLoop();
        void gpsRequestLoop();
        void ubxLoop();
        template<typename T>
        void sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force = false);
        bool sendTextAllWithRetry(AsyncWebSocket* ws, const String& text);
        void pingClients();
#ifdef MOWER_TERMINAL
        void sendTerminalLine(String line);
#endif
        void uploadStatusHandler(byte progress); 
      };
    }
  }
}