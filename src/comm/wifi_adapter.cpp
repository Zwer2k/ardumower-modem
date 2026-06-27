#include "wifi_adapter.h"
#include "log.h"
#include <WiFi.h>
#include <inttypes.h>

using namespace ArduMower::Modem::Wifi;

static const uint32_t staAbortTimeout = 1 * 60 * 1000;
static const uint32_t offStopTimeout = 5 * 60 * 1000;

void Adapter::reconnect()
{
  if (_mode != Mode::STA)
    return;

  Log(INFO, "WiFi::Adapter::reconnect");
  WiFi.reconnect();
}

void Adapter::fullReconnect()
{
  Log(WARN, "WiFi::Adapter::fullReconnect");
  // disconnect(false,true) statt disconnect(true,true):
  // wifioff=true triggert WiFi-Deinit/Reinit, das auf ESP32-S3+OPI-PSRAM
  // fehlschlägt ("create wifi task: failed", esp_wifi_init 257 = ESP_ERR_NO_MEM)
  WiFi.disconnect(false, true);

  if (_settings.wifi.staSettingsValid()) {
    beginSta();
  }
}

void Adapter::begin()
{
  _mode = mode();

  switch (_mode)
  {
  case Mode::STA:
    beginSta();
    break;

  case Mode::AP:
    beginAp();
    break;

  default:
    beginOff();
    break;
  }
}

void Adapter::loop()
{
  switch (_mode)
  {
  case Mode::STA:
    loopSta();
    break;

  case Mode::AP:
    loopAp();
    break;

  default:
    loopOff();
    break;
  }
}

void Adapter::loopSta()
{
  static bool wasEverConnected = false;
  static bool wasConnected = false;
  static uint32_t disconnectedSince = 0;
  const bool connected = WiFi.isConnected();

  if (!wasEverConnected && !connected && millis() - _staTimeoutStart > staAbortTimeout)
  {
    Log(INFO, "WiFi::Adapter::loopSta::no-connection");

    stop();
    _mode = Mode::OFF;
    beginOff();
    return;
  }

  if (connected == wasConnected)
  {
    // Prolonged disconnection after initial connect – force reconnect
    if (!connected && wasEverConnected && disconnectedSince != 0 &&
        millis() - disconnectedSince > 15000)
    {
      Log(INFO, "WiFi::Adapter::loopSta::reconnecting (disconnected %ums)", millis() - disconnectedSince);
      WiFi.reconnect();
      disconnectedSince = millis();
    }
    return;
  }

  wasConnected = connected;

  if (connected)
  {
    wasEverConnected = true;
    disconnectedSince = 0;
    auto ip = WiFi.localIP().toString();
    Log(INFO, "WiFi::Adapter::STA::IP(%s)", ip.c_str());
  }
  else
  {
    disconnectedSince = millis();
    Log(INFO, "WiFi::Adapter::STA::disconnected");
  }
}

void Adapter::loopAp()
{
}

void Adapter::loopOff()
{
  static bool stopped = false;
  static uint32_t stopFor = 0;

  if (stopFor != _offTimeoutStart)
  {
    stopped = false;
    stopFor = _offTimeoutStart;
  }

  if (stopped)
    return;

  if (millis() - _offTimeoutStart < offStopTimeout)
    return;

  stop();
  stopped = true;
  if (_settings.wifi.mode == Mode::STA && _settings.wifi.staSettingsValid())
  {
    Log(INFO, "WiFi::Adapter::loopOff::switch-to-sta");
    _mode = Mode::STA;
    beginSta();
    return;
  }

  Log(INFO, "WiFi::Adapter::loopOff::stop");
}

void Adapter::beginSta()
{
  Log(DBG, "WiFi::Adapter::beginSta");
  _staTimeoutStart = millis();
  WiFi.setHostname(_settings.general.name.c_str());
  WiFi.mode(WIFI_STA);

  if (_settings.wifi.sta_ip_mode == 1 && 
      _settings.wifi.sta_ip != "" && 
      _settings.wifi.sta_gateway != "" && 
      _settings.wifi.sta_subnet != "")
  {
    IPAddress ip, gateway, subnet, dns;
    if (ip.fromString(_settings.wifi.sta_ip.c_str()) && 
        gateway.fromString(_settings.wifi.sta_gateway.c_str()) && 
        subnet.fromString(_settings.wifi.sta_subnet.c_str()))
    {
      if (_settings.wifi.sta_dns != "" && dns.fromString(_settings.wifi.sta_dns.c_str()))
      {
        WiFi.config(ip, gateway, subnet, dns);
      }
      else
      {
        WiFi.config(ip, gateway, subnet);
      }
      Log(INFO, "WiFi::Adapter::STA::static-ip(%s)", _settings.wifi.sta_ip.c_str());
    }
    else
    {
      Log(WARN, "WiFi::Adapter::STA::invalid-static-ip");
    }
  }

  WiFi.begin(_settings.wifi.sta_ssid.c_str(), _settings.wifi.sta_psk.c_str());
  WiFi.setAutoReconnect(true);
}

void Adapter::beginAp()
{
  Log(DBG, "WiFi::Adapter::beginAp");
  WiFi.setHostname(_settings.general.name.c_str());
  WiFi.mode(WIFI_AP);
  if (_settings.wifi.apSettingsValid())
    WiFi.softAP(_settings.wifi.ap_ssid.c_str(), _settings.wifi.ap_psk.c_str());
  else
    WiFi.softAP(_settings.wifi.default_ap_ssid, _settings.wifi.default_ap_psk);

  auto ip = WiFi.softAPIP().toString();
  Log(INFO, "WiFi::Adapter::AP::IP(%s)", ip.c_str());
}

void Adapter::beginOff()
{
  Log(DBG, "WiFi::Adapter::beginOff");

  _offTimeoutStart = millis();
  beginAp();
}

void Adapter::stop()
{
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
}

Mode Adapter::mode()
{
  if (_settings.wifi.mode == Mode::STA)
  {
    if (_settings.wifi.staSettingsValid())
      return Mode::STA;

    Log(DBG, "WiFi::Adapter::mode::sta::settings-invalid")
  }

  if (_settings.wifi.mode == Mode::AP)
    return Mode::AP;

  return Mode::OFF;
}
