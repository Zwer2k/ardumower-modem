#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace ArduMower
{
  namespace Modem
  {
    class WebServer
    {      
    private:   
      AsyncWebServer *_server = new AsyncWebServer(80);

    public:
      WebServer() {}
      ~WebServer(){
        delete _server;
      }

      void begin();
      AsyncWebServer &server() { return *_server; }
    };
  }
}
