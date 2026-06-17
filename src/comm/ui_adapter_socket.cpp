#include <functional>
#include <time.h>
#include "ui_adapter_socket.h"
#ifdef ENABLE_MAP
#include "path_planner.h"
#endif
#include "json.h"
#include "log.h"
#include "logToUi.h"
#include "terminal.h"

#define _LOG_ "UiSocket::"

uint32_t clientPingInterval = 10000;
uint32_t defaultVersionRequestInterval = 10000;
uint32_t defaultStateUpdateInterval = 5000;

using namespace ArduMower::Modem::Http;

UiSocketItem::UiSocketItem(
  UiSocketHandler *socketHandler,
  AsyncWebSocketClient *client, 
  ArduMower::Domain::Robot::StateSource &source) 
  : _socketHandler(socketHandler), _client(client), _clientId(client->id()), _source(source) 
{
  _socketHandler->sendData(ResponseDataType::mowerState, this, true);
  yield();
  _socketHandler->sendData(ResponseDataType::desiredState, this, true);
  yield();
  _socketHandler->sendData(ResponseDataType::map, this, true);
  yield();
  _socketHandler->sendMapList(this);
  yield();
  _socketHandler->sendBufferedLogTo(this, 5);
  yield();
  _socketHandler->sendDrivenTrack(this);
#ifdef MOWER_TERMINAL
  _socketHandler->sendBufferedTerminalTo(this, 5);
#endif

  // Request fresh stats from Teensy immediately (bypasses rate limit)
  _socketHandler->requestStatsNow();

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
  // Send cached UBX data to new client if available
  if (_source.ubxResponse().timestamp > 0) {
    _socketHandler->sendData(ResponseDataType::ubxResponse, this, true);
  }
  if (_source.gpsDetails().timestamp > 0) {
    _socketHandler->sendData(ResponseDataType::gpsDetails, this, true);
  }
#endif
}

void UiSocketItem::handleData(RequestDataType dataType, JsonDocument &jsonData)
{
  Log(DBG, "%s handle data type %d", _LOG_, dataType);
  switch (dataType)
  {
  case RequestDataType::modemLogSettings:
    logToUi.modemLogLevel = jsonData["logLevel"];
    Log(INFO, "%s set modem log level to %d", _LOG_, logToUi.modemLogLevel);
    break;

  case RequestDataType::mowerConsoleRequest:
    _socketHandler->cmdToMower(jsonData["cmd"]);
    break;

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
  case RequestDataType::requestGpsDetails:
    _socketHandler->gpsDetailsRefCount++;
    _socketHandler->gpsDetailsActive = true;
    _socketHandler->ubxResponseActive = true;
    // Only reset polling timer on first activation (ref was 0)
    if (_socketHandler->gpsDetailsRefCount == 1) {
      _socketHandler->resetRequestTimestamp(ResponseDataType::gpsDetails);
      _socketHandler->ubxPollSequence = 0;
      _socketHandler->ubxConfigIndex = 0;
    }
    Log(INFO, "%s GPS details polling activated (ref=%d)", _LOG_, _socketHandler->gpsDetailsRefCount);
    break;

  case RequestDataType::stopGpsDetails:
    if (_socketHandler->gpsDetailsRefCount > 0) {
      _socketHandler->gpsDetailsRefCount--;
    }
    if (_socketHandler->gpsDetailsRefCount == 0) {
      _socketHandler->gpsDetailsActive = false;
      _socketHandler->ubxResponseActive = false;
    }
    Log(INFO, "%s GPS details polling deactivated (ref=%d)", _LOG_, _socketHandler->gpsDetailsRefCount);
    break;
#endif

  case RequestDataType::requestSensorSummary:
    _socketHandler->sensorSummaryRefCount++;
    _socketHandler->sensorSummaryActive = true;
    // Only reset polling timer on first activation (ref was 0)
    if (_socketHandler->sensorSummaryRefCount == 1) {
      _socketHandler->resetRequestTimestamp(ResponseDataType::sensorSummary);
    }
    Log(INFO, "%s Sensor summary polling activated (ref=%d)", _LOG_, _socketHandler->sensorSummaryRefCount);
    break;

  case RequestDataType::stopSensorSummary:
    if (_socketHandler->sensorSummaryRefCount > 0) {
      _socketHandler->sensorSummaryRefCount--;
    }
    if (_socketHandler->sensorSummaryRefCount == 0) {
      _socketHandler->sensorSummaryActive = false;
    }
    Log(INFO, "%s Sensor summary polling deactivated (ref=%d)", _LOG_, _socketHandler->sensorSummaryRefCount);
    break;

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
  case RequestDataType::requestUbx:
    {
      String hexCmd = jsonData["hex"] | "";
      if (hexCmd.length() > 0) {
        _socketHandler->sendUbx(hexCmd);
        _socketHandler->ubxResponseActive = true;
        _socketHandler->resetRequestTimestamp(ResponseDataType::ubxResponse);
        Log(INFO, "%s UBX command sent", _LOG_);
      }
    }
    break;
#endif

  case RequestDataType::joystickMove:
    {
      float linear = jsonData["linear"] | 0.0f;
      float angular = jsonData["angular"] | 0.0f;
      _socketHandler->joystickMove(linear, angular);
    }
    break;

  case RequestDataType::navigateTo:
    {
      float x = jsonData["x"] | 0.0f;
      float y = jsonData["y"] | 0.0f;
      _socketHandler->navigateTo(x, y);
    }
    break;

  case RequestDataType::uploadMap:
    _socketHandler->uploadMapToMower();
    break;

   case RequestDataType::setMap:
    {
      using namespace ArduMower::Domain::Robot;
      MowerMap map;
      Log(DBG, "%s setMap: parsing perimeter, exclusions, dockpoints, waypoints", _LOG_);
      auto readDouble = [](JsonObject p, const char* key1, const char* key2) -> double {
        if (p[key1].is<JsonVariant>()) return p[key1];
        if (p[key2].is<JsonVariant>()) return p[key2];
        return 0.0;
      };
      auto readPoint = [readDouble](JsonObject p) -> MapPoint {
        return MapPoint{readDouble(p, "X", "x"), -readDouble(p, "Y", "y")};
      };
      JsonArray perimeter = jsonData["perimeter"];
      for (JsonObject p : perimeter) {
        map.perimeter.push_back(readPoint(p));
      }
      JsonArray exclusions = jsonData["exclusions"];
      for (JsonArray ex : exclusions) {
        std::vector<MapPoint> excl;
        for (JsonObject p : ex) {
          excl.push_back(readPoint(p));
        }
        map.exclusions.push_back(excl);
      }
      JsonArray dockpoints = jsonData["dockpoints"];
      for (JsonObject p : dockpoints) {
        map.dockpoints.push_back(readPoint(p));
      }
      JsonArray waypoints = jsonData["waypoints"];
      for (JsonObject p : waypoints) {
        map.waypoints.push_back(readPoint(p));
      }
      map.rotation = jsonData["rotation"] | 0.0;
      Log(DBG, "%s setMap: parsed perimeter=%d exclusions=%d dockpoints=%d waypoints=%d rotation=%.1f", _LOG_,
          map.perimeter.size(), map.exclusions.size(), map.dockpoints.size(), map.waypoints.size(), map.rotation);
      _socketHandler->setMap(map);
      // Aktualisierte Karte sofort an alle verbundenen Clients senden.
      // Laufenden Chunk-Versand abbrechen, damit die neue Karte übertragen wird.
      _socketHandler->abortMapChunkSend();
      _socketHandler->sendData(ResponseDataType::map, NULL, true);
    }
    break;

   case RequestDataType::setMowSettings:
    {
      using namespace ArduMower::Domain::Robot;
      MowSettings s;
      s.pattern = jsonData["pattern"] | 0;
      s.width = jsonData["width"] | 0.3f;
      s.angle = jsonData["angle"] | 0;
      s.distanceToBorder = jsonData["distanceToBorder"] | 0;
      s.borderLaps = jsonData["borderLaps"] | 0;
      s.mowArea = jsonData["mowArea"] | true;
      s.mowExclusionBorder = jsonData["mowExclusionBorder"] | false;
      s.mowBorderCcw = jsonData["mowBorderCcw"] | false;
      _socketHandler->setMowSettings(s);
      _socketHandler->sendData(ResponseDataType::mowSettings, NULL, true);
    }
    break;

   case RequestDataType::requestMowSettings:
    _socketHandler->sendData(ResponseDataType::mowSettings, this, true);
    break;

   case RequestDataType::clearWaypoints:
    _socketHandler->clearWaypoints();
    break;

   case RequestDataType::calculateWaypoints:
    _socketHandler->calculateWaypoints();
    break;

   case RequestDataType::listMaps:
    _socketHandler->sendMapList(this);
    break;

   case RequestDataType::loadMap: {
    String id = jsonData["id"] | "";
    if (_source.loadMap(id)) {
      _socketHandler->abortMapChunkSend();
      _socketHandler->sendData(ResponseDataType::map, NULL, true);
      _socketHandler->sendMapList(NULL);
    }
    break;
  }

   case RequestDataType::saveMap: {
    String name = jsonData["name"] | "";
    double rotation = jsonData["rotation"] | 0.0;
    if (_source.saveMap(name, rotation).length() > 0) {
      _socketHandler->sendMapList(NULL);
    }
    break;
  }

   case RequestDataType::renameMap: {
    String id = jsonData["id"] | "";
    String name = jsonData["name"] | "";
    if (_source.renameMap(id, name)) {
      _socketHandler->sendMapList(NULL);
    }
    break;
  }

   case RequestDataType::deleteMap: {
    String id = jsonData["id"] | "";
    if (_source.deleteMap(id)) {
      _socketHandler->sendMapList(NULL);
    }
    break;
  }

  default:
    break;
  }
}

UiSocketItem::~UiSocketItem()
{
}

bool UiSocketItem::sendText(String text)
{
  auto* liveClient = _socketHandler->wsClient(_clientId);
  if (liveClient == nullptr) {
    _client = nullptr;
    return false;
  }
  if (liveClient != _client) {
    _client = liveClient;
  }
  if (_client == NULL || _client->status() == WS_DISCONNECTED)
    return false;

  if (!_client->canSend())
    return false;

  for (int attempt = 0; attempt < 3; ++attempt) {
    try {
      if (_client->text(text.c_str())) {
        _socketHandler->markClientActivity();
        return true;
      }
    } catch (...) {
      return false;
    }
    yield();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  return false;
}

void UiSocketItem::ping()
{
  if (_client != NULL && _client->status() != WS_DISCONNECTED)
    _client->ping();
}

AwsClientStatus UiSocketItem::status()
{
  if (_client == NULL)
    return WS_DISCONNECTED;
  return _client->status();
}


#ifdef MOWER_TERMINAL
UiSocketHandler::UiSocketHandler(
  Terminal &terminal,
  AsyncWebServer &server,
  ArduMower::Domain::Robot::StateSource &source,
  ArduMower::Domain::Robot::CommandExecutor &cmd,
  Ota::MowerUpdater &mowerUpdater
) 
  : _terminal(terminal), _server(server), _source(source), _cmd(cmd), _mowerUpdater(mowerUpdater)
{
  _ws = new AsyncWebSocket("/ws");

  for (int i=0; i < ResponseDataType::responseDataTypeLength; i++) {
    oldDataTimestamp[i] = 0;
    lastDataRequestTimestamp[i] = defaultStateUpdateInterval;
    lastSentTimestamp[i] = 0;
  }

  _terminal.addRxHandler(std::bind(&UiSocketHandler::sendTerminalLine, this, std::placeholders::_1));
  _mowerUpdater.addStatusHandler(std::bind(&UiSocketHandler::uploadStatusHandler, this, std::placeholders::_1));
}

void UiSocketHandler::sendTerminalLine(String line) 
{
  auto message = TerminalMessage(line);
  this->sendData(ResponseDataType::mowerConsole, NULL, message, false);
}
#else
UiSocketHandler::UiSocketHandler(
  AsyncWebServer &server,
  ArduMower::Domain::Robot::StateSource &source,
  ArduMower::Domain::Robot::CommandExecutor &cmd,
  Ota::MowerUpdater &mowerUpdater
)
  : _server(server), _source(source), _cmd(cmd), _mowerUpdater(mowerUpdater)
{
  _ws = new AsyncWebSocket("/ws");

  for (int i=0; i < ResponseDataType::responseDataTypeLength; i++) {
    oldDataTimestamp[i] = 0;
    lastDataRequestTimestamp[i] = defaultStateUpdateInterval;
    lastSentTimestamp[i] = 0;
  }

  _mowerUpdater.addStatusHandler(std::bind(&UiSocketHandler::uploadStatusHandler, this, std::placeholders::_1));
}
#endif

void UiSocketHandler::uploadStatusHandler(byte progress) 
{
  auto message = Ota::StatusMessage(progress);
  this->sendData(ResponseDataType::mowerConsole, NULL, message, false);
}

UiSocketHandler::~UiSocketHandler() 
{   
  delete _ws;
}

void UiSocketHandler::begin()
{
  _ws->onEvent(std::bind(&ArduMower::Modem::Http::UiSocketHandler::wsEvent, this, std::placeholders::_1, 
    std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
  _server.addHandler(_ws);

  // HTTP endpoint for CSV log export
  _server.on("/api/log/export", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String csv;
    logToUi.exportAll(csv);
    
    // Build filename with current date/time or fallback to millis
    char filename[64];
    time_t now = time(nullptr);
    if (now > 1609459200) { // After 2021-01-01, assume valid time
      struct tm* ti = localtime(&now);
      snprintf(filename, sizeof(filename), 
        "ardumower-log-%04d-%02d-%02d-%02d-%02d-%02d.csv",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour, ti->tm_min, ti->tm_sec);
    } else {
      snprintf(filename, sizeof(filename), "ardumower-log-%lu.csv", millis());
    }
    
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv; charset=utf-8", csv);
    response->addHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
    request->send(response);
  });
}

void UiSocketHandler::loop()
{
  _ws->cleanupClients();

  // Kartenliste bei Änderungen broadcasten, sobald mindestens ein Client verbunden ist
  if (_source.mapListDirty() && countConnectedClients() > 0) {
    _source.clearMapListDirty();
    sendMapList(NULL);
  }

  if (countConnectedClients() == 0)
    return;

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
  ubxLoop();
#endif

  static byte loopCase = 0;
    switch (loopCase)
    {
    case 0:
      versionRequestLoop();
      break;
    case 1:
      stateRequestLoop();
      break;
    case 2:
      sendData(ResponseDataType::mowerState);
      break;
    case 3:
      sendData(ResponseDataType::mowerStats);
      break;
    case 4:
      sendData(ResponseDataType::desiredState);
      break;
    case 5:
      processMapChunkSend();
      break;
    case 6:
      processCalculateWaypoints();
      break;
    case 7:
      sendData(ResponseDataType::map);
      break;
    case 8:
      logToUiLoop();
      break;
    case 9:
      sensorRequestLoop();
      break;
    case 10:
      if (_source.sensorSummary().timestamp > 0) {
        sendData(ResponseDataType::sensorSummary);
      }
      break;
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
    case 11:
      gpsRequestLoop();
      if (gpsDetailsActive) ubxPollLoop();
      break;
    case 12:
      if (_source.gpsDetails().timestamp > 0) {
        sendData(ResponseDataType::gpsDetails);
      }
      break;
    case 13:
      // Send UBX response immediately when available (no deduplication – frontend handles that)
      if (ubxResponseActive && _source.ubxResponse().timestamp > 0) {
        sendData(ResponseDataType::ubxResponse, NULL, true); // force=true: bypass oldDataTimestamp
        _source.ubxResponseP()->timestamp = 0; // Mark as consumed
        _lastSentUbxTimestamp = 0; // Allow next poll to advance immediately
      }
      break;
#endif
    case 14:
      processUploadToMower();
      break;
    case 15: {
      static unsigned long lastTrackSend = 0;
      if (millis() - lastTrackSend >= 10000) {
        lastTrackSend = millis();
        sendDrivenTrack(NULL);
      }
      break;
    }
    }
    loopCase++;
    if (loopCase > 15) loopCase = 0;
  yield();
  
  //pingClients();
}

void UiSocketHandler::versionRequestLoop()
{
  auto props = _source.props();
  if (props.timestamp != 0)
    return;

  if (millis() - lastVersionRequestTimestamp > defaultVersionRequestInterval) {
    _cmd.requestVersion();
    lastVersionRequestTimestamp = millis();
  }
}

void UiSocketHandler::stateRequestLoop()
{
  auto state = _source.state();

  if ((millis() - state.timestamp) > defaultStateUpdateInterval) {
    if ((lastDataRequestTimestamp[ResponseDataType::mowerState] > 0) && ((millis() - lastDataRequestTimestamp[ResponseDataType::mowerState]) < defaultStateUpdateInterval)) 
      return;
    _cmd.requestStatus();
    lastDataRequestTimestamp[ResponseDataType::mowerState] = millis();
  } 
}

void UiSocketHandler::requestStats()
{
  _cmd.requestStats();
}

void UiSocketHandler::requestStatsNow()
{
  _cmd.requestStatsNow();
}

void UiSocketHandler::sensorRequestLoop()
{
  if ((lastDataRequestTimestamp[ResponseDataType::sensorSummary] > 0) && ((millis() - lastDataRequestTimestamp[ResponseDataType::sensorSummary]) < 1000))
    return;
  _cmd.requestSensorSummary();
  lastDataRequestTimestamp[ResponseDataType::sensorSummary] = millis();
}

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
void UiSocketHandler::gpsRequestLoop()
{
  if ((lastDataRequestTimestamp[ResponseDataType::gpsDetails] > 0) && ((millis() - lastDataRequestTimestamp[ResponseDataType::gpsDetails]) < 20000))
    return;
  _cmd.requestGpsDetails();
  lastDataRequestTimestamp[ResponseDataType::gpsDetails] = millis();
}

void UiSocketHandler::ubxPollLoop()
{
  // Don't overwrite a pending command that hasn't been sent yet
  if (pendingUbxCmd.length() > 0) return;

  // Wait until the previous response was consumed by case 12
  if (ubxResponseActive && _source.ubxResponse().timestamp > 0) return;

  // Safety: advance even without response after timeout
  if (_lastSentUbxTimestamp > 0 && millis() - _lastSentUbxTimestamp < 5000) return;

  // Fast commands: polled every cycle for responsive Simple Dashboard
  static const char* fastCmds[] = {
    "B562010700000819",                   // 0: NAV-PVT
    "B5620135000036A3",                   // 1: NAV-SAT
  };

  // Slow config commands: one per cycle, cycled through
  static const char* slowCmds[] = {
    "B562010400000510",                   // 0: NAV-DOP
    "B5620A0400000E34",                   // 1: MON-VER
    "B5620A0900001343",                   // 2: MON-HW
    "B5620A38000042D0",                   // 3: MON-RF
    "B5620A36000040CA",                   // 4: MON-COMMS
    "B56201030000040D",                   // 5: NAV-STATUS
    "B562062400002A84",                   // 6: CFG-NAV5
    "B562060800000E30",                   // 7: CFG-RATE
    "B562068B080000000000010052402C79",   // 8: CFG-VALGET-PORT1
    "B562068B080000000000010052402C79",   // 9: CFG-VALGET-UART1-PROTO
    "B562068B0800000000003F000000D88D",   // 10: CFG-VALGET-GNSS
    "B562068B08000000000036100000DF99",   // 11: CFG-VALGET-SBAS
    "B562068B080000000000010091406BF7",   // 12: CFG-VALGET-RTCM
    "B562068B08000000000001002130EB07",   // 13: CFG-VALGET-RATE
  };
  const uint8_t slowCount = sizeof(slowCmds) / sizeof(slowCmds[0]);

  // Interleave: fast[0], fast[1], slow[n], fast[0], fast[1], slow[n+1], ...
  switch (ubxPollSequence) {
    case 0:
      pendingUbxCmd = fastCmds[0];  // NAV-PVT
      ubxPollSequence = 1;
      break;
    case 1:
      pendingUbxCmd = fastCmds[1];  // NAV-SAT
      ubxPollSequence = 2;
      break;
    case 2:
      pendingUbxCmd = slowCmds[ubxConfigIndex];
      ubxConfigIndex = (ubxConfigIndex + 1) % slowCount;
      ubxPollSequence = 0;
      break;
  }

  _source.ubxResponseP()->timestamp = 0;
  Log(DBG, "%subxPollLoop seq=%d configIdx=%d", _LOG_, ubxPollSequence, ubxConfigIndex);
}

bool UiSocketHandler::sendUbx(const String &hexCmd)
{
  pendingUbxCmd = hexCmd;
  _source.ubxResponseP()->timestamp = 0; // Discard stale/periodic frames
  _lastSentUbxTimestamp = 0;
  return true;
}
#endif

void UiSocketHandler::resetRequestTimestamp(ResponseDataType dataType)
{
  lastDataRequestTimestamp[dataType] = 0;
}

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
void UiSocketHandler::ubxLoop()
{
  if (pendingUbxCmd.length() == 0) return;

  static uint32_t lastAttempt = 0;
  if (millis() - lastAttempt < 200) return; // Throttle: ~5 retries/s max
  lastAttempt = millis();

  if (_cmd.sendUbx(pendingUbxCmd)) {
    Log(DBG, "%subxLoop sent pending cmd", _LOG_);
    pendingUbxCmd = "";
    _lastSentUbxTimestamp = millis();
  }
}
#endif

static void sanitizeUtf8InPlace(String& str);

void UiSocketHandler::sendData(ResponseDataType dataType, UiSocketItem *sendTo, bool force)
{
  switch (dataType) {
    case ResponseDataType::mowerState: {
      auto state = _source.state();
      // Position nur überschreiben, wenn der Mäher tatsächlich angedockt ist
      // (job == charge) oder gerade neu gestartet wurde und noch keine GPS-Position hat.
      // Nur amps < 0 ist zu unzuverlässig, da es zu oft vorkommt.
      bool dockedCharging = (state.job == 2);
      bool recentlyRestarted = (state.timestamp > 0 && state.timestamp < 30000);
      bool noPosition = (state.position.x == 0.0f && state.position.y == 0.0f);
      auto map = _source.mowerMap();
      bool overrideActive = (dockedCharging || (recentlyRestarted && noPosition)) && map.dockpoints.size() > 0;
      if (overrideActive) {
        const auto &home = map.dockpoints.back();
        state.position.x = home.X;
        state.position.y = home.Y;
        // Orientierung aus der Dock-Linie ableiten (Vektor vom vorletzten zum letzten Punkt)
        if (map.dockpoints.size() >= 2) {
          const auto &prev = map.dockpoints[map.dockpoints.size() - 2];
          state.position.delta = atan2(home.Y - prev.Y, home.X - prev.X);
        }
        // Nur beim Übergang in den Override-Zustand loggen, nicht dauernd währenddessen
        if (!_dockOverrideActive) {
          Log(DBG, "%s sendData: docked/restarted (job=%d ts=%u amps=%.2f), overriding position with home/dock (%.2f, %.2f)",
              _LOG_, state.job, state.timestamp, state.amps, state.position.x, state.position.y);
          _dockOverrideActive = true;
        }
      } else {
        if (_dockOverrideActive) {
          Log(DBG, "%s sendData: left dock/restart window (job=%d ts=%u amps=%.2f)",
              _LOG_, state.job, state.timestamp, state.amps);
          _dockOverrideActive = false;
        }
      }
      sendData(dataType, sendTo, state, force);
      if (state.job == 1 || state.job == 0) {
        pushDrivenTrackPoint(state.position.x, state.position.y, state.timestamp);
      }
      break;
    }
    case ResponseDataType::mowerStats:
      sendData(dataType, sendTo, _source.stats(), force);
      break;
    case ResponseDataType::desiredState:
      sendData(dataType, sendTo, _source.desiredState(), force);
      break;
    case ResponseDataType::map:
      // Starte asynchronen Map-Chunk-Versand
      startMapChunkSend(sendTo, force);
      break;
    case ResponseDataType::sensorSummary:
      sendData(dataType, sendTo, _source.sensorSummary(), force);
      break;
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
    case ResponseDataType::gpsDetails:
      sendData(dataType, sendTo, _source.gpsDetails(), force);
      break;
    case ResponseDataType::ubxResponse:
      sendData(dataType, sendTo, _source.ubxResponse(), force);
      break;
#endif
    case ResponseDataType::mowSettings:
      sendData(dataType, sendTo, _source.mowSettings(), force);
      break;
    default:
      break;
  }
}

// Starte asynchronen Map-Chunk-Versand
void UiSocketHandler::startMapChunkSend(UiSocketItem* sendTo, bool force) {
  auto map = _source.mowerMap();
  if (mapChunkSendState.active || (!force && (map.timestamp == 0 || map.timestamp == oldDataTimestamp[ResponseDataType::map]))) {
    return;
  }
  // reading-Flag setzen: schützt _map vor gleichzeitigen setMap()-Aufrufen
  map.beginRead();
  // Snapshot einmalig speichern - verhindert Race Condition waehrend Chunk-Versand
  mapChunkSendState.snapshot = map;
  // Metadaten des aktuellen Karteninhalts für alle Chunks merken
  mapChunkSendState.metaHash = _source.currentMapHash();
  mapChunkSendState.metaCrc = _source.currentMapCrc();
  mapChunkSendState.metaArea = _source.currentMapArea();
  mapChunkSendState.metaRotation = _source.currentMapRotation();
  lastDataRequestTimestamp[ResponseDataType::map] = 0;
  mapChunkSendState.active = true;
  mapChunkSendState.clientId = sendTo ? sendTo->clientId() : 0;
  mapChunkSendState.timestamp = map.timestamp;
  mapChunkSendState.phase = 0;
  mapChunkSendState.exclusionIdx = 0;
  mapChunkSendState.idx = 0;
}

// Pro loop() einen Chunk versenden – Snapshot aus mapChunkSendState verwenden
void UiSocketHandler::processMapChunkSend() {
  if (!mapChunkSendState.active) return;
  // Retry-Delay: bei fehlgeschlagenem Senden 100ms warten
  if (mapChunkSendState.lastRetryMs && millis() - mapChunkSendState.lastRetryMs < 100) return;
  // Keine Clients → Chunk-Versand abbrechen
  if (countConnectedClients() == 0) {
    Log(DBG, "%s processMapChunkSend: no connected clients, aborting", _LOG_);
    mapChunkSendState.active = false;
    mapChunkSendState.snapshot.endRead();
    return;
  }
  // Snapshot verwenden: einmalig beim Start gespeichert, bleibt konsistent
  auto& map = mapChunkSendState.snapshot;
  const size_t blockSize = 30;
  bool chunkSent = false;

  switch (mapChunkSendState.phase) {
    case 0: // Perimeter
      if (map.perimeter.size() > 0 && mapChunkSendState.idx < map.perimeter.size()) {
        chunkSent = sendMapChunk(MapPointType::Perimeter, map.perimeter, mapChunkSendState.timestamp, mapChunkSendState.clientId, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          mapChunkSendState.lastRetryMs = millis();
          break;
        }
        mapChunkSendState.idx += blockSize;
        if (mapChunkSendState.idx >= map.perimeter.size()) { mapChunkSendState.phase = 1; mapChunkSendState.idx = 0; }
        break;
      } else { mapChunkSendState.phase = 1; mapChunkSendState.idx = 0; }
      // fallthrough
    case 1: // Exclusions
      if (map.exclusions.size() > 0 && mapChunkSendState.exclusionIdx < map.exclusions.size()) {
        const auto& excl = map.exclusions[mapChunkSendState.exclusionIdx];
        if (excl.size() > 0 && mapChunkSendState.idx < excl.size()) {
          chunkSent = sendMapChunk(MapPointType::Exclusion, excl, mapChunkSendState.timestamp, mapChunkSendState.clientId, (int)mapChunkSendState.exclusionIdx, mapChunkSendState.idx, blockSize);
          if (!chunkSent) {
            mapChunkSendState.lastRetryMs = millis();
            break;
          }
          mapChunkSendState.idx += blockSize;
          if (mapChunkSendState.idx >= excl.size()) { mapChunkSendState.exclusionIdx++; mapChunkSendState.idx = 0; }
          break;
        } else { mapChunkSendState.exclusionIdx++; mapChunkSendState.idx = 0; }
        break;
      } else { mapChunkSendState.phase = 2; mapChunkSendState.exclusionIdx = 0; mapChunkSendState.idx = 0; }
      // fallthrough
    case 2: // Dockpoints
      if (map.dockpoints.size() > 0 && mapChunkSendState.idx < map.dockpoints.size()) {
        chunkSent = sendMapChunk(MapPointType::Dockpoints, map.dockpoints, mapChunkSendState.timestamp, mapChunkSendState.clientId, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          mapChunkSendState.lastRetryMs = millis();
          break;
        }
        mapChunkSendState.idx += blockSize;
        if (mapChunkSendState.idx >= map.dockpoints.size()) { mapChunkSendState.phase = 3; mapChunkSendState.idx = 0; }
        break;
      } else { mapChunkSendState.phase = 3; mapChunkSendState.idx = 0; }
      // fallthrough
    case 3: // Waypoints
      if (map.waypoints.size() > 0 && mapChunkSendState.idx < map.waypoints.size()) {
        chunkSent = sendMapChunk(MapPointType::Waypoints, map.waypoints, mapChunkSendState.timestamp, mapChunkSendState.clientId, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          mapChunkSendState.lastRetryMs = millis();
          break;
        }
        mapChunkSendState.idx += blockSize;
        if (mapChunkSendState.idx >= map.waypoints.size()) { mapChunkSendState.phase = 4; mapChunkSendState.idx = 0; }
        break;
      } else {
        sendMapChunk(MapPointType::Waypoints, map.waypoints, mapChunkSendState.timestamp, mapChunkSendState.clientId, -1, 0, 0);
        mapChunkSendState.phase = 4; mapChunkSendState.idx = 0;
      }
      // fallthrough
    case 4:
      mapChunkSendState.active = false;
      oldDataTimestamp[ResponseDataType::map] = map.timestamp;
      map.endRead();
      break;
  }
}

// Hilfsfunktion: Sende einen Chunk eines MapPoint-Vektors
bool UiSocketHandler::sendMapChunk(MapPointType pointType, const std::vector<ArduMower::Domain::Robot::MapPoint>& points, uint32_t timestamp, uint32_t clientId, int exclusionIdx, size_t startIdx, size_t blockSize) {
  const size_t maxJsonSize = 2048;
  size_t total = points.size();
  if (startIdx > total || (startIdx == total && total > 0)) return false;
  // Vor Serialisierung prüfen ob Ziel-Client sendefähig ist
  if (!clientCanSend(clientId)) return false;
  JsonDocument doc;
  doc["type"] = ResponseDataType::map;
  doc["timestamp"] = timestamp;
  auto dataObj = doc["data"].to<JsonObject>();
  dataObj["startIndex"] = (int)startIdx;
  dataObj["total"] = (int)total;
  dataObj["pointType"] = static_cast<int>(pointType);
  if (pointType == MapPointType::Exclusion) {
    dataObj["exclusionIdx"] = exclusionIdx;
  }
  if (mapChunkSendState.metaHash.length() > 0) {
    auto metaObj = dataObj["meta"].to<JsonObject>();
    metaObj["hash"] = mapChunkSendState.metaHash;
    metaObj["crc"] = mapChunkSendState.metaCrc;
    metaObj["area"] = mapChunkSendState.metaArea;
    metaObj["rotation"] = mapChunkSendState.metaRotation;
  }
  auto arr = dataObj["points"].to<JsonArray>();
  size_t measured = measureJson(doc);
  size_t pointsAdded = 0;
  size_t idx = startIdx;
  while (pointsAdded < blockSize && idx < total) {
    JsonObject obj = arr.add<JsonObject>();
    bool marshalOk = true;
    try {
      points[idx].marshal(obj);
    } catch (...) {
      Log(ERR, "%s marshal exception at pointType=%d idx=%u", _LOG_, (int)pointType, (unsigned)idx);
      marshalOk = false;
    }
    if (!marshalOk) {
      arr.remove(arr.size() - 1);
      ++idx;
      continue;
    }
    size_t newMeasured = measureJson(doc);
    if (newMeasured >= maxJsonSize - 128) {
      arr.remove(arr.size() - 1);
      Log(DBG, "%s remove point", _LOG_);
      break;
    }
    measured = newMeasured;
    ++idx;
    ++pointsAdded;
  }
  Log(DBG, "%s MapChunk type=%d exclIdx=%d: measured=%u, points=%u, nextIdx=%u, pointsAdded=%u", _LOG_, (int)pointType, exclusionIdx, (unsigned)measured, (unsigned)arr.size(), (unsigned)idx, (unsigned)pointsAdded);
  String stateStr;
  try {
    serializeJson(doc, stateStr);
    sanitizeUtf8InPlace(stateStr);
  } catch (...) {
    Log(ERR, "%s serializeJson failed", _LOG_);
    return false;
  }
  if (clientId > 0) {
    auto it = itemMap.find(clientId);
    if (it == itemMap.end()) {
      // client disconnected while transfer was in-flight – fall back to broadcast
      Log(DBG, "%s sendMapChunk: client %u gone, broadcasting", _LOG_, clientId);
      if (!sendTextAllWithRetry(_ws, stateStr)) {
        Log(ERR, "%s sendData failed", _LOG_);
        _ws->cleanupClients();
        return false;
      }
    } else if (!it->second->sendText(stateStr)) {
      Log(ERR, "%s sendData failed", _LOG_);
      _ws->cleanupClients();
      return false;
    }
  } else {
    if (!sendTextAllWithRetry(_ws, stateStr)) {
      Log(ERR, "%s sendData failed", _LOG_);
      _ws->cleanupClients();
      return false;
    }
  }
  return true;
}

void UiSocketHandler::logToUiLoop()
{
  if (!logToUi.hasData()) return;
  uint32_t now = millis();
  if (now - _lastLogSend < 100) return;

  for (auto &c : _ws->getClients()) {
    if (c.status() == WS_CONNECTED && !c.canSend())
      return;
  }

  _lastLogSend = now;
  sendData(ResponseDataType::modemLog, NULL, logToUi, false);
}

void UiSocketHandler::broadcastFlashProgress(size_t current, size_t total)
{
  int pct = (total > 0) ? (current * 100 / total) : 0;
  int clients = countConnectedClients();
  Log(DBG, "UiSocket::broadcastFlashProgress(pct=%d, clients=%d)", pct, clients);
  if (clients == 0) return;
  JsonDocument doc;
  auto status = doc["status"].to<JsonObject>();
  status["progress"] = pct;
  String json;
  serializeJson(doc, json);
  sendTextAllWithRetry(_ws, json);
}

#ifdef MOWER_TERMINAL
bool UiSocketHandler::cmdToMower(String cmd) {
  Log(DBG, "%s mower console cmd %s", _LOG_, cmd.c_str());
  bool success = _terminal.sendWithoutResponse(cmd);
  if (success) {
    Log(DBG, "%s sends cmd success", _LOG_);
  }
  return success;
}
#else
bool UiSocketHandler::cmdToMower(String) {
  return false;
}
#endif

void UiSocketHandler::joystickMove(float linear, float angular) {
  _cmd.manualDrive(linear, angular);
  Log(DBG, "%s joystickMove(%.2f, %.2f)", _LOG_, linear, angular);
}

void UiSocketHandler::navigateTo(float x, float y) {
  _cmd.navigateTo(x, y);
  Log(DBG, "%s navigateTo(%.2f, %.2f)", _LOG_, x, y);
}

void UiSocketHandler::sendProgress(String operation, int progress, String message) {
  _progressOp = operation;
  _progressPct = progress;
  _progressMsg = message;
}

void UiSocketHandler::uploadMapToMower() {
  if (_uploadToMowerPending) {
    Log(INFO, "%s uploadMapToMower: upload already in progress", _LOG_);
    return;
  }
  if (!_cmd.uploadMapToMower()) {
    Log(WARN, "%s uploadMapToMower: could not start upload", _LOG_);
    return;
  }
  _uploadToMowerPending = true;
  sendProgress("upload", 0, "Uploading map...");
  sendData(ResponseDataType::mowerState, NULL, true);
  Log(INFO, "%s uploadMapToMower: started", _LOG_);
}

void UiSocketHandler::processUploadToMower() {
  if (!_uploadToMowerPending) return;
  if (_cmd.uploadMapToMowerActive()) return;

  _uploadToMowerPending = false;
  if (_cmd.uploadMapToMowerSuccess()) {
    sendProgress("upload", 100, "Upload complete");
    Log(INFO, "%s processUploadToMower: upload complete, broadcasting map and state", _LOG_);
  } else {
    sendProgress("upload", 100, "Upload failed");
    Log(WARN, "%s processUploadToMower: upload failed", _LOG_);
  }
  sendData(ResponseDataType::map, NULL, true);
  sendData(ResponseDataType::mowerState, NULL, true);
  _cmd.requestStatusNow(); // refresh state/crc immediately after upload
}

void UiSocketHandler::setMowSettings(const ArduMower::Domain::Robot::MowSettings &s) {
  _source.setMowSettings(s);
}

void UiSocketHandler::sendWaypointsDirect(const std::vector<ArduMower::Domain::Robot::MapPoint> &waypoints, uint32_t timestamp) {
  JsonDocument doc;
  doc["type"] = ResponseDataType::map;
  doc["timestamp"] = timestamp;
  auto data = doc["data"].to<JsonObject>();
  data["startIndex"] = 0;
  data["total"] = (int)waypoints.size();
  data["pointType"] = static_cast<int>(MapPointType::Waypoints);
  auto arr = data["points"].to<JsonArray>();
  for (const auto &wp : waypoints) {
    JsonObject obj = arr.add<JsonObject>();
    wp.marshal(obj);
  }
  String json;
  serializeJson(doc, json);
  sanitizeUtf8InPlace(json);
  sendTextAllWithRetry(_ws, json);
}

void UiSocketHandler::clearWaypoints() {
  sendProgress("clear", 0, "Clearing waypoints...");
  sendData(ResponseDataType::mowerState, NULL, true);
  using namespace ArduMower::Domain::Robot;
  auto map = _source.mowerMap();
  uint32_t ts = millis();
  map.waypoints.clear();
  _source.setMap(map);
  abortMapChunkSend();
  sendWaypointsDirect(map.waypoints, ts);
  sendData(ResponseDataType::map, NULL, true);
  Log(INFO, "%s clearWaypoints: waypoints cleared, map broadcast", _LOG_);
  sendProgress("clear", 100, "Complete");
  sendData(ResponseDataType::mowerState, NULL, true);
}

void UiSocketHandler::calculateWaypoints() {
  if (_calculateWaypointsPending || _calculateWaypointsRunning) {
    Log(INFO, "%s calculateWaypoints: already queued or running", _LOG_);
    sendProgress("calculate", 0, "Calculation already in progress");
    return;
  }
  _calculateWaypointsPending = true;
  _calculateWaypointsTimestamp = millis();
  _calculateWaypointsMap = _source.mowerMap();
  _calculateWaypointsSettings = _source.mowSettings();
  sendProgress("calculate", 0, "Calculating waypoints...");
  sendData(ResponseDataType::mowerState, NULL, true);
  Log(INFO, "%s calculateWaypoints: queued", _LOG_);
}

void UiSocketHandler::processCalculateWaypoints() {
  if (!_calculateWaypointsPending || _calculateWaypointsRunning)
    return;

  _calculateWaypointsRunning = true;
  _calculateWaypointsPending = false;

  using namespace ArduMower::Domain::Robot;
  auto map = _calculateWaypointsMap;
  uint32_t ts = _calculateWaypointsTimestamp;

  map.waypoints.clear();
#ifdef ENABLE_MAP
  auto settings = _calculateWaypointsSettings;
  auto state = _source.state();
  decltype(ArduMower::Modem::PathPlanner::calculateWaypoints(map, settings, &state)) waypoints;
  try {
    waypoints = ArduMower::Modem::PathPlanner::calculateWaypoints(map, settings, &state);
  } catch (...) {
    Log(ERR, "%s processCalculateWaypoints: exception during calculation", _LOG_);
    sendProgress("calculate", 100, "Failed");
    _calculateWaypointsRunning = false;
    return;
  }
  for (const auto &wp : waypoints)
    map.waypoints.push_back(wp);
#endif
  _source.setMap(map);
  abortMapChunkSend();
  sendWaypointsDirect(map.waypoints, ts);
  sendData(ResponseDataType::map, NULL, true);
  Log(INFO, "%s processCalculateWaypoints: %d waypoints generated, map broadcast", _LOG_, map.waypoints.size());
  sendProgress("calculate", 100, "Complete");
  sendData(ResponseDataType::mowerState, NULL, true);
  _calculateWaypointsRunning = false;
}

void UiSocketHandler::setMap(const ArduMower::Domain::Robot::MowerMap &map) {
  _source.setMap(map);
}

void UiSocketHandler::abortMapChunkSend() {
  if (mapChunkSendState.active) {
    mapChunkSendState.active = false;
    Log(DBG, "%s abortMapChunkSend: ongoing chunk send aborted", _LOG_);
  }
}

static bool sendJsonDoc(UiSocketItem *item, JsonDocument &doc)
{
  String stateStr;
  serializeJson(doc, stateStr);
  if (stateStr.length() == 0) return false;
  sanitizeUtf8InPlace(stateStr);
  return item->sendText(stateStr);
}

void UiSocketHandler::sendBufferedLogTo(UiSocketItem* item, uint16_t maxChunks)
{
  uint16_t chunks = 0;
  uint16_t offset = 0;
  const uint16_t chunkSize = 20;
  while (chunks < maxChunks) {
    JsonDocument doc;
    doc["type"] = ResponseDataType::modemLog;
    uint16_t sent = logToUi.marshalBatch(doc["data"].to<JsonObject>(), offset, chunkSize);
    if (sent == 0 || doc.overflowed()) break;
    if (!sendJsonDoc(item, doc)) break;
    offset += sent;
    chunks++;
    yield();
  }
  oldDataTimestamp[ResponseDataType::modemLog] = logToUi.timestamp;
}

#ifdef MOWER_TERMINAL
void UiSocketHandler::sendBufferedTerminalTo(UiSocketItem* item, uint16_t maxChunks)
{
  uint16_t chunks = 0;
  uint16_t offset = 0;
  const uint16_t chunkSize = 20;
  while (chunks < maxChunks) {
    JsonDocument doc;
    doc["type"] = ResponseDataType::mowerConsole;
    uint16_t sent = _terminal.marshalBatch(doc["data"].to<JsonObject>(), offset, chunkSize);
    if (sent == 0 || doc.overflowed()) break;
    if (!sendJsonDoc(item, doc)) break;
    offset += sent;
    chunks++;
    yield();
  }
}
#endif

static void sanitizeUtf8InPlace(String& str) {
  for (size_t i = 0; i < str.length(); i++) {
    unsigned char c = (unsigned char)str[i];
    if (c < 0x20 && c != '\r' && c != '\n' && c != '\t')
      goto needs_work;
    if (c >= 0x80)
      goto needs_work;
  }
  return;

needs_work:
  String output;
  output.reserve(str.length());
  for (size_t i = 0; i < str.length(); i++) {
    unsigned char c = (unsigned char)str[i];
    if (c < 0x20 && c != '\r' && c != '\n' && c != '\t')
      output += '?';
    else if (c >= 0x80)
      output += '?';
    else
      output += (char)c;
  }
  str = output;
}

template<typename T>
void UiSocketHandler::sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force)
{
  if (!force && (data.timestamp == 0 || data.timestamp == oldDataTimestamp[dataType])) {
    return;
  }

  // Rate limit sends per data type (skip if sent too recently)
  if (!force) {
    uint32_t now = millis();
    uint32_t minInterval = 0;
    switch (dataType) {
      case ResponseDataType::mowerState:     minInterval = 1000;  break;
      case ResponseDataType::mowerStats:     minInterval = 5000;  break;
      case ResponseDataType::desiredState:   minInterval = 2000;  break;
      case ResponseDataType::sensorSummary:  minInterval = 1000;  break;
      case ResponseDataType::modemLog:       minInterval = 100;   break;
      default:                               minInterval = 0;     break;
    }
    if (minInterval > 0 && (now - lastSentTimestamp[dataType]) < minInterval)
      return;
    lastSentTimestamp[dataType] = now;
  }

  oldDataTimestamp[dataType] = data.timestamp;
  // Don't reset request timestamp for types with their own polling loop,
  // otherwise they would re-request immediately after sending data
  switch (dataType) {
    case ResponseDataType::mowerState:
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
    case ResponseDataType::gpsDetails:
#endif
    case ResponseDataType::sensorSummary:
      break;
    default:
      lastDataRequestTimestamp[dataType] = 0;
      break;
  }

  JsonDocument doc;
  doc["type"] = dataType;
  auto _j = doc["data"].to<JsonObject>(); data.marshal(_j);

  if (dataType == ResponseDataType::mowerState && !_progressOp.isEmpty()) {
    doc["progressPct"] = _progressPct;
    doc["progressMsg"] = _progressMsg;
    doc["progressOp"] = _progressOp;
    if (_progressPct >= 100) {
      _progressPct = 0;
      _progressOp = "";
      _progressMsg = "";
    }
  }

  String stateStr;
  serializeJson(doc, stateStr);
  sanitizeUtf8InPlace(stateStr);

  if (sendTo != NULL) {
    sendTo->sendText(stateStr);
  } else {
    if (countConnectedClients() == 0) return;
    if (!sendTextAllWithRetry(_ws, stateStr)) {
      _ws->cleanupClients();
    }
  }
}

void UiSocketHandler::sendMapList(UiSocketItem *sendTo)
{
  auto list = _source.mapList();
  String activeId = _source.activeMapId();

  JsonDocument doc;
  doc["type"] = ResponseDataType::mapList;
  doc["timestamp"] = millis();
  auto dataObj = doc["data"].to<JsonObject>();
  dataObj["activeId"] = activeId;
  auto mapsArr = dataObj["maps"].to<JsonArray>();
  for (const auto &m : list) {
    JsonObject obj = mapsArr.add<JsonObject>();
    obj["id"] = m.id;
    obj["name"] = m.name;
    obj["area"] = m.area;
    obj["hash"] = m.hash;
    obj["crc"] = m.crc;
    obj["rotation"] = m.rotation;
    obj["timestamp"] = m.timestamp;
  }

  String json;
  serializeJson(doc, json);
  sanitizeUtf8InPlace(json);

  if (sendTo != NULL) {
    sendTo->sendText(json);
  } else {
    if (countConnectedClients() == 0) return;
    if (!sendTextAllWithRetry(_ws, json)) {
      _ws->cleanupClients();
    }
  }
}

void UiSocketHandler::pushDrivenTrackPoint(float x, float y, uint32_t timestamp) {
  _track.push(x, y, timestamp);
}

void UiSocketHandler::sendDrivenTrack(UiSocketItem *sendTo)
{
  if (countConnectedClients() == 0 && sendTo == NULL) return;
  if (_track.size() == 0) return;

  JsonDocument doc;
  doc["type"] = ResponseDataType::drivenTrack;
  doc["timestamp"] = millis();
  auto dataObj = doc["data"].to<JsonObject>();
  _track.marshal(dataObj);

  String json;
  serializeJson(doc, json);
  sanitizeUtf8InPlace(json);

  if (sendTo != NULL) {
    sendTo->sendText(json);
  } else {
    if (!sendTextAllWithRetry(_ws, json)) {
      _ws->cleanupClients();
    }
  }
}

bool UiSocketHandler::sendTextAllWithRetry(AsyncWebSocket *ws, const String &text)
{
  // Entferne verwaiste Clients (z.B. TCP tot aber WS-Status noch CONNECTED),
  // bevor wir Nachrichten in deren interne Queue stellen.
  ws->cleanupClients();

  bool anySent = false;
  int clientCount = 0;
  bool anyConnected = false;
  try {
    for (auto &client : ws->getClients()) {
      if (++clientCount > 16)
        break;
      auto status = client.status();
      if (status != WS_CONNECTED)
        continue;
      anyConnected = true;
      if (!client.canSend())
        continue;
      try {
        if (client.text(text.c_str())) {
          anySent = true;
        }
      } catch (...) {
      }
      yield();
    }
  } catch (...) {
    return false;
  }
  if (anySent) markClientActivity();
  if (!anyConnected) return true;
  return anySent;
}

bool UiSocketHandler::clientCanSend(uint32_t clientId) {
  if (clientId == 0) {
    // Broadcast: prüfe ob irgendein Client sendefähig ist
    for (auto &c : _ws->getClients()) {
      if (c.status() == WS_CONNECTED && c.canSend()) return true;
    }
    return false;
  }
  for (auto &c : _ws->getClients()) {
    if (c.id() == clientId) return c.status() == WS_CONNECTED && c.canSend();
  }
  return false;
}

size_t UiSocketHandler::countConnectedClients()
{
  size_t count = 0;
  for (auto &c : _ws->getClients()) {
    if (c.status() == WS_CONNECTED) count++;
    if (count > 0) break;
  }
  return count;
}

void UiSocketHandler::pingClients()
{
  if (millis() - lastclientPing < clientPingInterval)
    return;

  //_ws->pingAll();
  
  for (std::map<uint32_t, UiSocketItem*>::iterator it = itemMap.begin(); it != itemMap.end(); ++it)
  {
    if (it->second->status() == WS_CONNECTED) {
      it->second->ping();
    } else {
      Log(DBG, "%s client %u not more connected\n", _LOG_, it->first); 
      if (itemMap.find(it->first) != itemMap.end()) {
        delete itemMap[it->first];
        itemMap.erase(it->first);
      } 
    }
  }

  lastclientPing = millis();
}

void UiSocketHandler::wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if(type == WS_EVT_CONNECT){
    //client connected
    markWsConnectionEvent();
    Log(INFO, "%s ws[%s][%u] connect\n", _LOG_, server->url(), client->id());

    // Nicht bei Queue-Überlauf trennen – sonst erzeugt jeder Log-Sturm
    // eine Reconnect-Schleife. Lieber Nachrichten verwerfen.
    client->setCloseClientOnQueueFull(false);
    
    JsonDocument doc;
    doc["type"] = ResponseDataType::responseHello;
    doc["client"] = client->id();
    JsonObject valueDescriptions = doc["data"].to<JsonObject>();
    JsonObject newObj = valueDescriptions["job"].to<JsonObject>();
    int i = 0; 
    for (const char *item: ArduMower::Domain::Robot::State::State::jobDesc) {
      newObj[String(i++)] = item;
    }

    newObj = valueDescriptions["posSolution"].to<JsonObject>();
    i = 0;
    for (const char *item: ArduMower::Domain::Robot::State::State::posSolutionDesc) {
      newObj[String(i++)] = item;
    }

    valueDescriptions["logLevel"] = logToUi.modemLogLevel;

#ifdef ENABLE_MAP
    valueDescriptions["mapEnabled"] = true;
#endif
#ifdef ENABLE_LIVE_MAP
    valueDescriptions["liveMapEnabled"] = true;
#endif
#ifdef ENABLE_GPS_DASHBOARD
    valueDescriptions["gpsDashboardEnabled"] = true;
#endif

    String dataStr;
    serializeJson(doc, dataStr);
    sanitizeUtf8InPlace(dataStr);
    Log(DBG, "%s %s", _LOG_, dataStr.c_str());
    client->text(dataStr.c_str());

    client->ping();
    if (itemMap.find(client->id()) != itemMap.end()) {
      delete itemMap[client->id()];
      itemMap.erase(client->id());
    }
    _frameBuffer.erase(client->id());
    itemMap[client->id()] = new UiSocketItem(this, client, _source);

    _ws->cleanupClients(); // cleanup disconnected clients
  } else if(type == WS_EVT_ERROR){
    markWsConnectionEvent();
    Log(ERR, "%s ws[%s][%u] error(%u): %s\n", _LOG_, server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    _frameBuffer.erase(client->id());
    if (itemMap.find(client->id()) != itemMap.end()) {
      if (mapChunkSendState.clientId == client->id())
        mapChunkSendState.clientId = 0;
      delete itemMap[client->id()];
      itemMap.erase(client->id());
    }
    // Clean up ref counts if this was the last client
    if (itemMap.empty()) {
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
      gpsDetailsRefCount = 0;
      gpsDetailsActive = false;
      ubxResponseActive = false;
#endif
      sensorSummaryRefCount = 0;
      sensorSummaryActive = false;
      Log(INFO, "%s last client removed on error, resetting all ref counts", _LOG_);
    }
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    markWsConnectionEvent();
    Log(INFO, "%s ws[%s][%u] disconnect\n", _LOG_, server->url(), client->id());
    _frameBuffer.erase(client->id());
    if (itemMap.find(client->id()) != itemMap.end()) {
      if (mapChunkSendState.clientId == client->id())
        mapChunkSendState.clientId = 0;
      delete itemMap[client->id()];
      itemMap.erase(client->id());
    }
    // Clean up ref counts if this was the last client
    if (itemMap.empty()) {
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
      gpsDetailsRefCount = 0;
      gpsDetailsActive = false;
      ubxResponseActive = false;
#endif
      sensorSummaryRefCount = 0;
      sensorSummaryActive = false;
      Log(INFO, "%s last client disconnected, resetting all ref counts", _LOG_);
    }
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    Log(ERR, "%s ws[%s][%u] error(%u): %s\n", _LOG_, server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    markClientActivity();
    //pong message was received (in response to a ping request maybe)
    Log(DBG, "%s ws[%s][%u] pong[%u]: %s\n", _LOG_, server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    markClientActivity();
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Log(DBG, "%s ws[%s][%u] %s-message[%llu]: ", _LOG_, server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        Log(DBG, "%s %s\n", _LOG_, (char*)data);
      } else {
        String logLine = _LOG_;
        for(size_t i=0; i < info->len; i++){
          logLine += String(data[i], HEX) + " ";
        }
        logLine += "\n";
        Log(DBG, "%s", logLine.c_str());
      }
      if(info->opcode == WS_TEXT) 
      {
        handleData(client->id(), (char*)data);
      }
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        _frameBuffer[client->id()] = String();
        if(info->num == 0)
          Log(DBG, "%s ws[%s][%u] %s-message start\n", _LOG_, server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Log(DBG, "%s ws[%s][%u] frame[%u] start[%llu]\n", _LOG_, server->url(), client->id(), info->num, info->len);
      }

      Log(DBG, "%s ws[%s][%u] frame[%u] %s[%llu - %llu]: ", _LOG_, server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Log(DBG, "%s %s\n", _LOG_, (char*)data);
        String frag((const char*)data, len);
        if (_frameBuffer.find(client->id()) != _frameBuffer.end()) {
          _frameBuffer[client->id()].concat(frag);
        }
      } else {
        String logLine = _LOG_;
        for(size_t i=0; i < len; i++){
          logLine += String(data[i], HEX) + " ";
        }
        logLine += "\n";
        Log(DBG, "%s", logLine.c_str());
      }

      if((info->index + len) == info->len){
        Log(DBG, "%s ws[%s][%u] frame[%u] end[%llu]\n", _LOG_, server->url(), client->id(), info->num, info->len);
        if(info->final){
          Log(DBG, "%s ws[%s][%u] %s-message end\n", _LOG_, server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT && _frameBuffer.find(client->id()) != _frameBuffer.end())
          {
            String &buf = _frameBuffer[client->id()];
            handleData(client->id(), (char*)buf.c_str());
            _frameBuffer.erase(client->id());
          }
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void UiSocketHandler::handleData(uint32_t clientId, char *data) 
{
  // Reichlich bemessener Puffer: 4x rawLen, da ArduinoJson für tief verschachtelte
  // Strukturen (Karte mit vielen Stützpunkten) mehr Speicher braucht als das JSON selbst.
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Log(ERR, "%s handleData: deserializeJson failed: %s (rawLen=%u)", _LOG_, error.f_str(), (unsigned)strlen(data));
    return;
  }

  if (itemMap.find(clientId) != itemMap.end())
  {
    const size_t rawLen = strlen(data);
    Log(DBG, "%s handleData type=%d rawLen=%u", _LOG_, (int)doc["type"], (unsigned)rawLen);
    JsonDocument jsonData;
    if (!jsonData.set(doc["data"])) {
      Log(ERR, "%s handleData: jsonData.set() failed", _LOG_);
      return;
    }
    itemMap[clientId]->handleData(doc["type"], jsonData);
  } 
  else 
  {
    Log(ERR, "%s client with id %u doesn't exist", _LOG_, clientId);
  }
}
