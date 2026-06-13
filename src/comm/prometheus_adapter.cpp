#include "prometheus_adapter.h"
#include "prometheus.h"
#include "log.h"

using namespace ArduMower::Modem::Prometheus;

Adapter::Adapter(Settings::Settings &settings,
                 AsyncWebServer &server,
                 ArduMower::Domain::Robot::StateSource &source,
                 ArduMower::Domain::Robot::CommandExecutor &cmd)
    : _settings(settings), _server(server), _source(source), _cmd(cmd)
{
}

void Adapter::begin()
{
  if (!_settings.prometheus.enabled)
    return;

  auto props = _source.propsP();
  auto state = _source.stateP();
  auto statistics = _source.statsP();

  registerSystemProperties();
  registerRobotProperties(props);
  registerRobotState(state);
  registerRobotStatistics(statistics);

  _server.on("/metrics", HTTP_GET, [this](AsyncWebServerRequest* request) { metrics(request); });
}

void Adapter::metrics(AsyncWebServerRequest *request)
{
  auto send = [&](ArduMower::Domain::Robot::Stats::Stats stats,
                  ArduMower::Domain::Robot::State::State status,
                  bool stale)
  {
    Log(DBG, "Prometheus::Adapter::metrics::send stale=%d", stale);
    unsigned int size = 1;
    auto &all = ArduMower::Modem::Prometheus::allMeasurements();
    for (auto it : all)
      size += it->length();
    char *buffer = (char *)malloc(size + 1);
    if (buffer == nullptr)
    {
      request->send(500, "text/plain", "no heap");
      return;
    }
    unsigned int index = 0;
    for (auto it : all)
    {
      index += it->write(&buffer[index], size - index);
    }
    request->_tempObject = buffer;
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (const unsigned char *)buffer, index);
    if (stale)
      response->addHeader("X-Prometheus-Stats-Stale", "true");
    request->send(response);
  };

  auto status = _source.state();
  auto stats = _source.stats();
  const uint32_t now = millis();
  const bool statsFresh = (now - stats.timestamp < 15000);

  if (!statsFresh)
  {
    static uint32_t lastStatsRequest = 0;
    if (now - lastStatsRequest >= 500)
    {
      Log(DBG, "Prometheus::Adapter::metrics::request-refresh");
      _cmd.requestStats();
      lastStatsRequest = now;
    }
  }

  send(stats, status, !statsFresh);
}
