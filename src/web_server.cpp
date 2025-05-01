#include "web_server.h"
#include "log.h"

void ArduMower::Modem::WebServer::begin()
{
  _server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    Log(DBG, "Req");
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  
  _server->begin();
}
