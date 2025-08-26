#include <functional>
#include "ui_adapter_socket.h"
#include "json.h"
#include "log.h"
#include "terminal.h"

#define _LOG_ "UiSocket::"

uint32_t clientPingInterval = 10000;
uint32_t defaultVersionRequestInterval = 10000;
uint32_t defaultStateUpdateInterval = 10000;

using namespace ArduMower::Modem::Http;

UiSocketItem::UiSocketItem(
  UiSocketHandler *socketHandler,
  AsyncWebSocketClient *client, 
  ArduMower::Domain::Robot::StateSource &source) 
  : _socketHandler(socketHandler), _client(client), _source(source) 
{
  _socketHandler->sendData(ResponseDataType::mowerState, this, true);
  _socketHandler->sendData(ResponseDataType::desiredState, this, true);
  _socketHandler->sendData(ResponseDataType::map, this, true);
}

void UiSocketItem::handleData(RequestDataType dataType, DynamicJsonDocument &jsonData)
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
  
  default:
    break;
  }
}

UiSocketItem::~UiSocketItem() 
{
}

bool UiSocketItem::sendText(String text)
{
  // Bis zu 10 Versuche, jeweils 100ms Pause zwischen den Versuchen
  for (int attempt = 0; attempt < 10; ++attempt) {
    try {
      if (_client->text(text.c_str())) {
        return true;
      }
    } catch (...) {
      return false;
    }    
    yield();
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms warten
  }
  return false;
}

void UiSocketItem::ping()
{
  _client->ping();
}

AwsClientStatus UiSocketItem::status()
{
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
}

void UiSocketHandler::loop()
{
  if (_ws->count() == 0)
    return;

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
    sendData(ResponseDataType::desiredState);
    break;
  case 4:
    // Map-Chunk-Prozess
    processMapChunkSend();
    break;
  case 5:
    sendData(ResponseDataType::map);
    break;
  case 6:
    logToUiLoop();
    break;
  default:
    loopCase = 0;
    break;
  }
  loopCase++;
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

void UiSocketHandler::sendData(ResponseDataType dataType, UiSocketItem *sendTo, bool force)
{
  switch (dataType) {
    case ResponseDataType::mowerState:
      sendData(dataType, sendTo, _source.state(), force);
      break;
    case ResponseDataType::desiredState:
      sendData(dataType, sendTo, _source.desiredState(), force);
      break;
    case ResponseDataType::map:
      // Starte asynchronen Map-Chunk-Versand
      startMapChunkSend(sendTo, force);
      break;
    default:
      break;
  }
}

// Starte asynchronen Map-Chunk-Versand
void UiSocketHandler::startMapChunkSend(UiSocketItem* sendTo, bool force) {
  auto map = _source.mowerMap();
  if ((map.timestamp == 0) || (!force && (map.timestamp == oldDataTimestamp[ResponseDataType::map])) || mapChunkSendState.active) {
    return;
  }
  map.beginRead();
  lastDataRequestTimestamp[ResponseDataType::map] = 0;
  mapChunkSendState.active = true;
  mapChunkSendState.sendTo = sendTo;
  mapChunkSendState.timestamp = map.timestamp;
  mapChunkSendState.phase = 0;
  mapChunkSendState.exclusionIdx = 0;
  mapChunkSendState.idx = 0;
}

// Pro loop() einen Chunk versenden
void UiSocketHandler::processMapChunkSend() {
  if (!mapChunkSendState.active) return;
  auto map = _source.mowerMap();
  const size_t blockSize = 30;
  String stateStr;
  bool chunkSent = false;

  switch (mapChunkSendState.phase) {
    case 0: // Perimeter
      if (map.perimeter.size() > 0 && mapChunkSendState.idx < map.perimeter.size()) {
        chunkSent = sendMapChunk(MapPointType::Perimeter, map.perimeter, mapChunkSendState.timestamp, mapChunkSendState.sendTo, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          // Fehler beim Senden: Sendevorgang von vorne beginnen
          mapChunkSendState.phase = 0;
          mapChunkSendState.exclusionIdx = 0;
          mapChunkSendState.idx = 0;
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
          chunkSent = sendMapChunk(MapPointType::Exclusion, excl, mapChunkSendState.timestamp, mapChunkSendState.sendTo, (int)mapChunkSendState.exclusionIdx, mapChunkSendState.idx, blockSize);
          if (!chunkSent) {
            mapChunkSendState.phase = 0;
            mapChunkSendState.exclusionIdx = 0;
            mapChunkSendState.idx = 0;
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
        chunkSent = sendMapChunk(MapPointType::Dockpoints, map.dockpoints, mapChunkSendState.timestamp, mapChunkSendState.sendTo, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          mapChunkSendState.phase = 0;
          mapChunkSendState.exclusionIdx = 0;
          mapChunkSendState.idx = 0;
          break;
        }
        mapChunkSendState.idx += blockSize;
        if (mapChunkSendState.idx >= map.dockpoints.size()) { mapChunkSendState.phase = 3; mapChunkSendState.idx = 0; }
        break;
      } else { mapChunkSendState.phase = 3; mapChunkSendState.idx = 0; }
      // fallthrough
    case 3: // Waypoints
      if (map.waypoints.size() > 0 && mapChunkSendState.idx < map.waypoints.size()) {
        chunkSent = sendMapChunk(MapPointType::Waypoints, map.waypoints, mapChunkSendState.timestamp, mapChunkSendState.sendTo, -1, mapChunkSendState.idx, blockSize);
        if (!chunkSent) {
          mapChunkSendState.phase = 0;
          mapChunkSendState.exclusionIdx = 0;
          mapChunkSendState.idx = 0;
          break;
        }
        mapChunkSendState.idx += blockSize;
        if (mapChunkSendState.idx >= map.waypoints.size()) { mapChunkSendState.phase = 4; mapChunkSendState.idx = 0; }
        break;
      } else { mapChunkSendState.phase = 4; mapChunkSendState.idx = 0; }
      // fallthrough
    case 4:
      mapChunkSendState.active = false;
      oldDataTimestamp[ResponseDataType::map] = map.timestamp;
      map.endRead();
      break;
  }
}

// Hilfsfunktion: Sende einen Chunk eines MapPoint-Vektors
bool UiSocketHandler::sendMapChunk(MapPointType pointType, const std::vector<ArduMower::Domain::Robot::MapPoint>& points, uint32_t timestamp, UiSocketItem* sendTo, int exclusionIdx, size_t startIdx, size_t blockSize) {
  const size_t maxJsonSize = 2048;
  size_t total = points.size();
  if (startIdx >= total) return false;
  DynamicJsonDocument doc(maxJsonSize);
  doc["type"] = ResponseDataType::map;
  doc["timestamp"] = timestamp;
  auto dataObj = doc.createNestedObject("data");
  dataObj["startIndex"] = (int)startIdx;
  dataObj["total"] = (int)total;
  dataObj["pointType"] = static_cast<int>(pointType);
  if (pointType == MapPointType::Exclusion) {
    dataObj["exclusionIdx"] = exclusionIdx;
  }
  auto arr = dataObj.createNestedArray("points");
  size_t measured = measureJson(doc);
  size_t pointsAdded = 0;
  size_t idx = startIdx;
  while (pointsAdded < blockSize && idx < total) {
    JsonObject obj = arr.createNestedObject();
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
  } catch (...) {
    Log(ERR, "%s serializeJson failed", _LOG_);
    return false;
  }
  if (sendTo != NULL) {
    if (!sendTo->sendText(stateStr)) {
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
  if (logToUi.hasData()) {
    sendData(ResponseDataType::modemLog, NULL, logToUi, false);
  }
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

template<typename T>
void UiSocketHandler::sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force)
{
  if ((data.timestamp == 0) || (!force && (data.timestamp == oldDataTimestamp[dataType]))) {
    return;  
  }
    
  oldDataTimestamp[dataType] = data.timestamp;
  lastDataRequestTimestamp[dataType] = 0;

  DynamicJsonDocument doc(4096);
  doc["type"] = dataType;
  data.marshal(doc.createNestedObject("data"));
  
  String stateStr;
  serializeJson(doc, stateStr);
  
  if (sendTo != NULL) {
    sendTo->sendText(stateStr);
  } else {
    if (!sendTextAllWithRetry(_ws, stateStr)) {
      Log(DBG, "%ssendData cleanup old clients\n", _LOG_); 
      _ws->cleanupClients(); // cleanup disconnected clients
    }
  }
}

bool UiSocketHandler::sendTextAllWithRetry(AsyncWebSocket* ws, const String& text) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    try {
      auto status = ws->textAll(text);
      if (status == AsyncWebSocket::ENQUEUED) {
        return true;
      }
    } catch (...) {
      return false;
    }
    yield();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  return false;
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
    Log(INFO, "%s ws[%s][%u] connect\n", _LOG_, server->url(), client->id());
    
    DynamicJsonDocument doc(1024);
    doc["type"] = ResponseDataType::responseHello;
    doc["client"] = client->id();
    JsonObject valueDescriptions = doc.createNestedObject("data");
    JsonObject newObj = valueDescriptions.createNestedObject("job");
    int i = 0; 
    for (const char *item: ArduMower::Domain::Robot::State::State::jobDesc) {
      newObj[String(i++)] = item;
    }

    newObj = valueDescriptions.createNestedObject("posSolution");
    i = 0;
    for (const char *item: ArduMower::Domain::Robot::State::State::posSolutionDesc) {
      newObj[String(i++)] = item;
    }

    valueDescriptions["logLevel"] = logToUi.modemLogLevel;

    String dataStr;
    serializeJson(doc, dataStr);
    Log(DBG, "%s %s", _LOG_, dataStr.c_str());
    client->text(dataStr.c_str());

    client->ping();
    itemMap[client->id()] = new UiSocketItem(this, client, _source);

    _ws->cleanupClients(); // cleanup disconnected clients
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    Log(INFO, "%s ws[%s][%u] disconnect\n", _LOG_, server->url(), client->id());
    if (itemMap.find(client->id()) != itemMap.end()) {
      delete itemMap[client->id()];
      itemMap.erase(client->id());
    }
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    Log(ERR, "%s ws[%s][%u] error(%u): %s\n", _LOG_, server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    Log(DBG, "%s ws[%s][%u] pong[%u]: %s\n", _LOG_, server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
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
        if(info->num == 0)
          Log(DBG, "%s ws[%s][%u] %s-message start\n", _LOG_, server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Log(DBG, "%s ws[%s][%u] frame[%u] start[%llu]\n", _LOG_, server->url(), client->id(), info->num, info->len);
      }

      Log(DBG, "%s ws[%s][%u] frame[%u] %s[%llu - %llu]: ", _LOG_, server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Log(DBG, "%s %s\n", _LOG_, (char*)data);
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
          if(info->message_opcode == WS_TEXT)
          {
            client->text("I got your text message");
            handleData(client->id(), (char*)data);
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
  DynamicJsonDocument doc(strlen(data) * 2);
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Log(ERR, "%s deserializeJson() failed: %s", _LOG_, error.f_str());
    return;
  }

  if (itemMap.find(clientId) != itemMap.end())
  {
    DynamicJsonDocument jsonData(doc["data"].size());
    jsonData = doc["data"];
    itemMap[clientId]->handleData(doc["type"], jsonData);
  } 
  else 
  {
    Log(ERR, "%s client with id ", _LOG_, clientId, " don't exists");
  }
}
