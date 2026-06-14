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
        requestLogExport,
        joystickMove,
        navigateTo,
        setMap,
        uploadMap,
        setMowSettings,
        requestMowSettings,
        clearWaypoints,
        calculateWaypoints,
        listMaps,
        loadMap,
        saveMap,
        renameMap,
        deleteMap,
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
        logExport,
        mowSettings,
        operationProgress,
        mapList,
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
        void handleData(RequestDataType dataType, JsonDocument &jsonData);
        bool sendText(String text);
        void ping();
        AwsClientStatus status();
        uint32_t clientId() { return _clientId; }
        ~UiSocketItem();
      
      private:
        UiSocketHandler *_socketHandler;
        AsyncWebSocketClient *_client;
        uint32_t _clientId;
        ArduMower::Domain::Robot::StateSource &_source;
      };

      struct MapChunkSendState {
        bool active = false;
        uint32_t clientId = 0;
        uint32_t timestamp = 0;
        int phase = 0;
        size_t exclusionIdx = 0;
        size_t idx = 0;
        ArduMower::Domain::Robot::MowerMap snapshot;  // Einmalige Kopie der Map beim Start
        String metaHash;
        double metaArea = 0.0;
        double metaRotation = 0.0;
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
        void sendMapList(UiSocketItem *sendTo = NULL);
        void broadcastFlashProgress(size_t current, size_t total);
        void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
        bool sendUbx(const String &hexCmd);
#endif
        void resetRequestTimestamp(ResponseDataType dataType);
        void joystickMove(float linear, float angular);
        void navigateTo(float x, float y);
        void sendProgress(String operation, int progress, String message = "");
        void requestStats();
        void requestStatsNow();
        void sendBufferedLogTo(UiSocketItem* item, uint16_t maxChunks = 0xFFFF);
        void setMap(const ArduMower::Domain::Robot::MowerMap &map);
        void setMowSettings(const ArduMower::Domain::Robot::MowSettings &s);
        void clearWaypoints();
        void calculateWaypoints();
        void processCalculateWaypoints();
        void uploadMapToMower();
        void processUploadToMower();
        void abortMapChunkSend();
        void sendWaypointsDirect(const std::vector<ArduMower::Domain::Robot::MapPoint> &waypoints, uint32_t timestamp);
        size_t clientCount() { return countConnectedClients(); }
        uint32_t lastClientActivity() const { return _lastClientActivity; }
        uint32_t lastWsConnectionEvent() const { return _lastWsConnectionEvent; }
        void markClientActivity() { _lastClientActivity = millis(); }
        void markWsConnectionEvent() { _lastWsConnectionEvent = millis(); }
#ifdef MOWER_TERMINAL
        void sendBufferedTerminalTo(UiSocketItem* item, uint16_t maxChunks = 0xFFFF);
#endif
        AsyncWebSocketClient* wsClient(uint32_t clientId) { return _ws->client(clientId); }

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
        bool gpsDetailsActive = false;
#endif
        bool sensorSummaryActive = false;
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
        bool ubxResponseActive = false;
        String pendingUbxCmd;

        // UBX-Polling: fast commands (NAV-PVT, NAV-SAT) interleaved with slow config commands
        uint8_t ubxPollSequence = 0;     // 0=NAV-PVT, 1=NAV-SAT, 2=slow config
        uint8_t ubxConfigIndex = 0;      // cycles through slow config commands (0-13)

        // Reference counting for multi-client support
        uint32_t gpsDetailsRefCount = 0;
#endif
        uint32_t sensorSummaryRefCount = 0;

      private:
        void startMapChunkSend(UiSocketItem* sendTo, bool force);
        void processMapChunkSend();
        bool sendMapChunk(MapPointType pointType, const std::vector<ArduMower::Domain::Robot::MapPoint>& points, uint32_t timestamp, uint32_t clientId, int exclusionIdx, size_t startIdx, size_t blockSize);
        AsyncWebSocket *_ws;

        uint32_t lastVersionRequestTimestamp = 0;
        uint32_t oldDataTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastDataRequestTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastSentTimestamp[ResponseDataType::responseDataTypeLength];
        uint32_t lastclientPing = 0;
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
        uint32_t _lastSentUbxTimestamp = 0;
#endif
        
        int _progressPct = 0;
        String _progressOp;
        String _progressMsg;
        uint32_t _lastLogSend = 0;
        uint32_t _lastClientActivity = 0;
        uint32_t _lastWsConnectionEvent = 0;
        bool _dockOverrideActive = false;

        volatile bool _uploadToMowerPending = false;
        volatile bool _calculateWaypointsPending = false;
        volatile bool _calculateWaypointsRunning = false;
        uint32_t _calculateWaypointsTimestamp = 0;
        ArduMower::Domain::Robot::MowerMap _calculateWaypointsMap;
        ArduMower::Domain::Robot::MowSettings _calculateWaypointsSettings;

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
        std::map<uint32_t, String> _frameBuffer;
        
        void versionRequestLoop();
        void stateRequestLoop();
        void sensorRequestLoop();
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
        void gpsRequestLoop();
        void ubxLoop();
        void ubxPollLoop();
#endif
        template<typename T>
        void sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force = false);
        bool sendTextAllWithRetry(AsyncWebSocket* ws, const String& text);
        size_t countConnectedClients();
        void pingClients();
#ifdef MOWER_TERMINAL
        void sendTerminalLine(String line);
#endif
        void uploadStatusHandler(byte progress); 
      };
    }
  }
}