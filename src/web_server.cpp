#include "web_server.h"
#include "log.h"

void ArduMower::Modem::WebServer::begin()
{
  _server->begin();
}
