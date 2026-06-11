#include "ota_arduinoota.h"
#include <ArduinoOTA.h>
#include "log.h"
#include <esp_task_wdt.h>

using namespace ArduMower::Modem::Ota;

void ArduinoOta::begin()
{
  _active = false;
  ArduinoOTA.setHostname(_settings.general.name.c_str());
  ArduinoOTA
      .onStart([&]()
               { Log(INFO, "Ota::ArduinoOta::start");
               _active = true; })
      .onEnd([&]()
             { Log(INFO, "Ota::ArduinoOta::end");
             _active = false; })
      .onError([&](ota_error_t error)
               { Log(ERR, "Ota::ArduinoOta::error(%d)", (int)error);
               _active = false; });

  ArduinoOTA.begin();
}

void ArduinoOta::loop()
{
  ArduinoOTA.handle();
}
