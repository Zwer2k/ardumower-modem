#include "ui_adapter_socket.h"
#include "json.h"

uint32_t defaultStateUpdateInterval = 10000;

using namespace ArduMower::Modem::Http;

UiSocketItem::UiSocketItem(
  UiSocketHandler *socketHandler,
  AsyncWebSocketClient *client, 
  ArduMower::Domain::Robot::StateSource &source) 
  : _socketHandler(socketHandler), _client(client), _source(source) 
{
  _socketHandler->sendData(DataType::mowerState, this);
  _socketHandler->sendData(DataType::desiredState, this);
}

void UiSocketItem::handleData(DataType dataType, DynamicJsonDocument &jsonData)
{
  Log(INFO, "handle data type %d", dataType);
}

UiSocketItem::~UiSocketItem() 
{   
}

void UiSocketItem::sendText(String text)
{
  _client->printf(text.c_str());
}

UiSocketHandler::UiSocketHandler(
  ArduMower::Domain::Robot::StateSource &source,
  ArduMower::Domain::Robot::CommandExecutor &cmd) 
  : _source(source), _cmd(cmd)
{
  for (int i; i < DataType::dataType_length; i++) {
    oldDataTimestamp[i] = 0;
  }
}

UiSocketHandler::~UiSocketHandler() 
{   
}

void UiSocketHandler::loop()
{
   stateRequestLoop();
   sendData(DataType::mowerState);
   sendData(DataType::desiredState);
}

void UiSocketHandler::stateRequestLoop()
{
  auto state = _source.state();

  if ((uint32_t)(millis() - defaultStateUpdateInterval) > state.timestamp) {
    if ((newDataRequestTimestamp > 0) && ((uint32_t)(millis() - defaultStateUpdateInterval) < newDataRequestTimestamp)) 
      return;
    _cmd.requestStatus();
    newDataRequestTimestamp = millis();
  } 
}

void UiSocketHandler::sendData(DataType dataType, UiSocketItem *sendTo)
{
  switch (dataType) {
    case DataType::mowerState:
      sendData(sendTo, dataType, _source.state());
      break;
    case DataType::desiredState:
      sendData(sendTo, dataType, _source.desiredState());
      break;
  }
}

template<typename T>
void UiSocketHandler::sendData(UiSocketItem *sendTo, DataType dataType, T data)
{
  if ((data.timestamp == 0) || (data.timestamp == oldDataTimestamp[DataType::mowerState])) {
    return;  
  }

  newDataRequestTimestamp = 0;

  DynamicJsonDocument doc(1024);
  doc["type"] = dataType; 
  data.marshal(doc.createNestedObject("data"));

  String stateStr;
  serializeJson(doc, stateStr);
  Log(INFO, stateStr.c_str());

  if (sendTo != NULL) {
    sendTo->sendText(stateStr);
  } else {
    for (auto it = itemMap.begin(); it != itemMap.end(); ++it) {
      Log(INFO, "send to client %d %s", it->first, stateStr.c_str()); 
      it->second->sendText(stateStr);
    }
  }

  oldDataTimestamp[dataType] = data.timestamp;
}

void UiSocketHandler::wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if(type == WS_EVT_CONNECT){
    //client connected
    Log(INFO, "ws[%s][%u] connect\n", server->url(), client->id());
    DynamicJsonDocument doc(1024);
    doc["type"] = DataType::hello;
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

    String dataStr;
    serializeJson(doc, dataStr);
    Log(INFO, dataStr.c_str());
    client->text(dataStr.c_str());

    client->ping();
    itemMap[client->id()] = new UiSocketItem(this, client, _source);
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    Log(INFO, "ws[%s][%u] disconnect\n", server->url(), client->id());
    if (itemMap.find(client->id()) != itemMap.end()) {
      delete itemMap[client->id()];
      itemMap.erase(client->id());
    }
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    Log(INFO, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    Log(INFO, "ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Log(INFO, "ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        Log(INFO, "%s\n", (char*)data);
      } else {
        for(size_t i=0; i < info->len; i++){
          Log(INFO, "%02x ", data[i]);
        }
        Log(INFO, "\n");
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
          Log(INFO, "ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Log(INFO, "ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Log(INFO, "ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Log(INFO, "%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          Log(INFO, "%02x ", data[i]);
        }
        Log(INFO, "\n");
      }

      if((info->index + len) == info->len){
        Log(INFO, "ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Log(INFO, "ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
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
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  if (itemMap.find(clientId) != itemMap.end())
  {
    DynamicJsonDocument jsonData = doc["data"];
    itemMap[clientId]->handleData(doc["type"], jsonData);
  } 
  else 
  {
    Log(ERR, "client with id ", clientId, " don't exists");
  }
}
