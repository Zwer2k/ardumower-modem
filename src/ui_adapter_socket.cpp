#include "ui_adapter_socket.h"
#include "json.h"
#include "log.h"

#define _LOG_ "UiSocket::"

uint32_t clientPingInterval = 10000;
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
  return _client->text(text.c_str());
}

void UiSocketItem::ping()
{
  _client->ping();
}

AwsClientStatus UiSocketItem::status()
{
  return _client->status();
}

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

  stateRequestLoop();
  sendData(ResponseDataType::mowerState);
  sendData(ResponseDataType::desiredState);
  logToUiLoop();
  //pingClients();
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
    default:
      break;
  }
}

void UiSocketHandler::logToUiLoop()
{
  if (logToUi.hasData()) {
    sendData(ResponseDataType::modemLog, NULL, logToUi, false);
  }
}

bool UiSocketHandler::cmdToMower(String cmd) {
  Log(DBG, "%s mower console cmd %s", _LOG_, cmd.c_str());
  bool success = _terminal.sendWithoutResponse(cmd);
  if (success) {
    Log(DBG, "%s sends cmd success", _LOG_);
  }
  return success;
}

template<typename T>
void UiSocketHandler::sendData(ResponseDataType dataType, UiSocketItem *sendTo, T data, bool force)
{
  if ((data.timestamp == 0) || (!force && (data.timestamp == oldDataTimestamp[dataType]))) {
    return;  
  }
    
  oldDataTimestamp[dataType] = data.timestamp;
  lastDataRequestTimestamp[dataType] = 0;

  DynamicJsonDocument doc(1024);
  doc["type"] = dataType;
  data.marshal(doc.createNestedObject("data"));
  
  String stateStr;
  serializeJson(doc, stateStr);
  
  if (sendTo != NULL) {
    sendTo->sendText(stateStr);
  } else {
    auto status = _ws->textAll(stateStr);    
    if (status != AsyncWebSocket::ENQUEUED) {
      Log(DBG, "%ssendData cleanup old clients\n", _LOG_); 
      _ws->cleanupClients(); // cleanup disconnected clients
    }
  }
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
