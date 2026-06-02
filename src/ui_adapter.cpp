#include "ui_adapter.h"
#include "asset_bundle.h"
#include "log.h"
#include "settings.h"
#include "json.h"
#include <ArduinoJson.h>

using namespace ArduMower::Modem::Http;

UiAdapter::UiAdapter(Api::Api &api,
                     Settings::Settings &settings,
                     AsyncWebServer &server,
                     ArduMower::Domain::Robot::StateSource &source,
                     ArduMower::Domain::Robot::CommandExecutor &cmd)
    : Common(settings), _api(api), _settings(settings), _server(server), _source(source), _cmd(cmd)
{
}

UiAdapter::~UiAdapter()
{
}

void UiAdapter::begin()
{
  
  _server.on("/api/modem/info", HTTP_GET, std::bind(&UiAdapter::handleApiGetModemInfo, this, std::placeholders::_1));
  _server.on("/api/modem/status", HTTP_GET, std::bind(&UiAdapter::handleApiGetModemStatus, this, std::placeholders::_1));

  _server.on("/api/modem/settings", HTTP_GET, std::bind(&UiAdapter::handleApiGetModemSettings, this, std::placeholders::_1));
  _server.on("/api/modem/settings/reset", HTTP_POST, std::bind(&UiAdapter::handleApiResetModemSettings, this, std::placeholders::_1));
  // free happens in ~AsyncWebServer
  auto settingsHandler = new AsyncCallbackJsonWebHandler("/api/modem/settings", std::bind(&UiAdapter::handleApiPostModemSettings, this, std::placeholders::_1, std::placeholders::_2));
  settingsHandler->setMethod(HTTP_POST);
  _server.addHandler(settingsHandler);

  _server.on("/api/modem/bluetooth/reset", HTTP_POST, std::bind(&UiAdapter::handleApiResetModemBluetoothPairings, this, std::placeholders::_1));

  _server.on("/api/robot/desired_state", HTTP_GET, std::bind(&UiAdapter::handleApiGetRobotDesiredState, this, std::placeholders::_1));

  auto commandHandler = new AsyncCallbackJsonWebHandler("/api/robot/command", std::bind(&UiAdapter::handleApiPostRobotCommand, this, std::placeholders::_1, std::placeholders::_2));
  commandHandler->setMethod(HTTP_POST);
  _server.addHandler(commandHandler);
 
  _server.onNotFound(std::bind(&UiAdapter::handleRequest, this, std::placeholders::_1));
}

void UiAdapter::loop()
{
  if (_restartAt != 0 && millis() >= _restartAt)
  {
    _restartAt = 0;
    ESP.restart();
  }
}

bool UiAdapter::servePath(AsyncWebServerRequest *request, const String &path)
{
  for (auto i = 0; i < asset_count; i++)
  {
    const asset_t *asset = &assets[i];
    if (path != asset->path)
      continue;

    if (request->hasHeader("If-None-Match"))
    {
      auto h = request->getHeader("If-None-Match");
      if (h != nullptr && h->value() == asset->etag)
      {
        request->send(304);
        Log(DBG, "UiAdapter::servePath::304(path=%s)", path.c_str());
        return true;
      }
    }

    auto *response = request->beginResponse(200, asset->mime, asset->data, asset->size);
    response->addHeader("Connection", "close");
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("ETag", asset->etag);
    response->addHeader("Last-Modified", asset->time);

    if (strcmp(asset->path, "/index.html") == 0 || strcmp(asset->path, "/_app/version.json") == 0) {
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    } else if (strstr(asset->path, "/_app/immutable/") != nullptr) {
      response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    }

    request->send(response);
    Log(DBG, "UiAdapter::servePath::200(path=%s size=%d)", path.c_str(), asset->size);
    return true;
  }

  return false;
}

void UiAdapter::handleRequest(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;

  String name = request->url();
  if (name == "/")
    name = "/index.html";

  if (servePath(request, name))
    return;

  if (name != "/index.html" && servePath(request, "/index.html"))
    return;

  Log(DBG, "UiAdapter::handleRequest::404(path=%s)", name.c_str());
  String err = "not found: " + name;
  request->send(404, "text/plain", err);
}

void UiAdapter::handleApiGetModemInfo(AsyncWebServerRequest *request)
{
  // explicitly allowed without auth
  AsyncJsonResponse *response = new AsyncJsonResponse();
  JsonObject root = response->getRoot();
  ArduMower::Modem::Settings::Properties.marshal(root);
  response->setLength();
  request->send(response);
}

void UiAdapter::handleApiGetModemStatus(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;

  AsyncJsonResponse *response = new AsyncJsonResponse();
  const JsonObject &root = response->getRoot();
  root["uptime"] = millis();

  response->setLength();
  request->send(response);
}

void UiAdapter::handleApiGetModemSettings(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonDocument doc(2048);
  const JsonObject &o = doc.to<JsonObject>();
  _settings.marshal(o);
  _settings.stripSecrets(o);
  serializeJson(doc, *response);
  request->send(response);
}

void UiAdapter::handleApiPostModemSettings(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (!auth(request))
    return;

  Settings::Settings uploaded = _settings;
  if (!uploaded.unmarshal(json.as<JsonObject>()))
  {
    reject(request, 400, "decode", "unmarshal");
    return;
  }

  String invalid;
  if (!uploaded.valid(invalid))
  {
    reject(request, 400, "validate", invalid);
    return;
  }

  if (!uploaded.save())
  {
    Log(ERR, "UiAdapter::handleApiPostModemSettings::500(save)");
    reject(request, 500, "save", "unknown-error");
    return;
  }

  DynamicJsonDocument doc(1024);
  uploaded.marshal(doc.to<JsonObject>());
  String body;
  serializeJson(doc, body);

  request->send(200, "application/json", body);

  _restartAt = millis() + 1000;
}

void UiAdapter::handleApiResetModemSettings(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;

  Settings::Settings replace(_settings.filename());
  if (!replace.save())
  {
    request->send(500, "text/plain", "save error");
    return;
  }

  DynamicJsonDocument doc(1024);
  replace.marshal(doc.to<JsonObject>());
  String body;
  serializeJson(doc, body);
  request->send(200, "application/json", body);

  _restartAt = millis() + 1000;
}

void UiAdapter::handleApiGetRobotDesiredState(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;

  auto desiredState = _source.desiredState();
  auto res = ArduMower::Domain::Json::encode(desiredState);
  request->send(200, "application/json", res);
}

void UiAdapter::handleApiResetModemBluetoothPairings(AsyncWebServerRequest *request)
{
  if (!auth(request))
    return;
  
  _api.ble->clearPairings();
  request->send(200, "application/json", "{\"result\":\"ok\"}");

  _restartAt = millis() + 1000;
}

void UiAdapter::handleApiPostRobotCommand(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (!auth(request))
    return;

  String action = json.as<JsonObject>()["action"] | "";
  bool ok = false;

  if (action == "start")
    ok = _cmd.start();
  else if (action == "stop")
    ok = _cmd.stop();
  else if (action == "dock")
    ok = _cmd.dock();
  else if (action == "reboot")
    ok = _cmd.reboot();
  else if (action == "poweroff")
    ok = _cmd.powerOff();
  else if (action == "skipWaypoint")
    ok = _cmd.skipWaypoint();
  else if (action == "requestStatus") {
    if (_source.state().timestamp == 0)
      _cmd.requestStatus(); // cache empty → forward to Teensy
    ok = true;
  } else if (action == "requestStats") {
    if (_source.stats().timestamp == 0)
      _cmd.requestStats(); // cache empty → forward to Teensy
    ok = true;
  }
  else if (action == "mowerEnabled")
    ok = _cmd.mowerEnabled(json.as<JsonObject>()["enabled"] | true);
  else if (action == "sonarEnabled")
    ok = _cmd.sonarEnabled(json.as<JsonObject>()["enabled"] | true);
  else if (action == "finishAndRestartEnabled")
    ok = _cmd.finishAndRestartEnabled(json.as<JsonObject>()["enabled"] | true);
  else if (action == "changeSpeed")
    ok = _cmd.changeSpeed(json.as<JsonObject>()["speed"] | 0.2f);
  else if (action == "setFixTimeout")
    ok = _cmd.setFixTimeout(json.as<JsonObject>()["timeout"] | 0);
  else if (action == "customCmd")
    ok = _cmd.customCmd(json.as<JsonObject>()["cmd"] | "");
  else {
    reject(request, 400, "command", "unknown action: " + action);
    return;
  }

  if (action == "requestStatus") {
    auto state = _source.state();
    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    doc["action"] = action;
    state.marshal(doc.createNestedObject("data"));
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
  } else if (action == "requestStats") {
    auto stats = _source.stats();
    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    doc["action"] = action;
    stats.marshal(doc.createNestedObject("data"));
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
  } else {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(256);
    doc["success"] = ok;
    doc["action"] = action;
    serializeJson(doc, *response);
    request->send(response);
  }
}

