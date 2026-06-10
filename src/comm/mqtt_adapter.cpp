#include "mqtt_adapter.h"
#include "mqtt_ha.h"
#include "mqtt_iobroker.h"
#include "json.h"
#include "log.h"
#include "url.h"
#include <ArduinoJson.h>

#define _LOG_ "MqttAdapter::"

using namespace ArduMower::Modem;

#define mqttStatusInterval 5000
#define mqttStatsInterval 10000
#define mqttPropsInterval 86400000

void MqttAdapter::begin()
{
  if (!settings.mqtt.enabled)
    return;

  Log(INFO, "%sbegin", _LOG_);
  ArduMower::Util::URL url(settings.mqtt.server);
  int port = url.port();
  if (port == -1)
    port = 1883;

  client.begin(url.hostname().c_str(), port, net);
  client.onMessage(std::bind(&MqttAdapter::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2));

  if (settings.mqtt.iob) iob.setMQTTClient(&client);
}

void MqttAdapter::loop(const uint32_t now)
{
  if (!settings.mqtt.enabled)
    return;

  if (!handleConnection(now)) { // Connection check costs a lot of time therefore loop and connect combined
    return;
  }

  static byte publishSelector = 0;
  switch (publishSelector++) { // handle mqtt.loop and publishing only every 20 loops
    case 0:
      publishState(now);
      break;

    case 1:
      publishProps(now);
      break;

    case 2:
      publishStats(now);
      break;

    case 3:
      if (settings.mqtt.ha) ha.loop(now); // MQTT loop takes a lot of time, so it should not be executed every time
      break;
    
    default:
      if (publishSelector >= 20) publishSelector = 0;
  }
}

static uint32_t betterTime(uint32_t userSettingSeconds, uint32_t minimumMilliseconds)
{
  const uint32_t user = 1000 * userSettingSeconds;
  if (user < minimumMilliseconds)
    return minimumMilliseconds;

  return user;
}

void MqttAdapter::publishState(const uint32_t now)
{
  if (!settings.mqtt.publishStatus)
    return;
  static uint32_t next_time = 0;
  if (now < next_time)
    return;

  auto state = source.state();

  const uint32_t interval = betterTime(settings.mqtt.publishInterval, mqttStatusInterval);

  if (now - state.timestamp > interval)
  {
    static uint32_t last_time = 0;
    if (now - last_time < 1000)
      return;

    last_time = now;
    if (!cmd.requestStatus()) {
      next_time = now + mqttStatusInterval;
      return;
    }

    Log(DBG, "%spublishState::refresh-requested(%d)", _LOG_, now - state.timestamp);
    return;
  }

  if (state.timestamp == 0)
    return;

  Log(DBG, "%spublishState", _LOG_);
  if ((settings.mqtt.publishFormat == 1) || (settings.mqtt.publishFormat == 3)) {
    String json = ArduMower::Domain::Json::encode(state);
    if (!client.publish(topic("/state").c_str(), json.c_str()))
      return;
  }
  if ((settings.mqtt.publishFormat == 2) || (settings.mqtt.publishFormat == 3)) {
    DynamicJsonDocument doc(1024);
    JsonObject object = doc.to<JsonObject>();
    state.marshal(object);
    if (!publishWithSubtopics(object, topic("/state"))) {
      return;
    }
  }

  if (settings.mqtt.iob) iob.publishState(state);
  next_time = now + interval;
}

void MqttAdapter::publishProps(const uint32_t now)
{
  if (!settings.mqtt.publishStatus)
    return;

  static uint32_t next_time = 0;
  static uint32_t last_published = 0;
  auto props = source.props();
  if (props.timestamp == last_published && now < next_time)
    return;

  if (props.timestamp == 0)
    return;

  String json = ArduMower::Domain::Json::encode(props);
  Log(DBG, "%spublishProps", _LOG_);
  if ((settings.mqtt.publishFormat == 1) || (settings.mqtt.publishFormat == 3)) {
    if (!client.publish(topic("/props").c_str(), json.c_str()))
      return;
  }
  if ((settings.mqtt.publishFormat == 2) || (settings.mqtt.publishFormat == 3)) {
    DynamicJsonDocument doc(1024);
    JsonObject object = doc.to<JsonObject>();
    props.marshal(object);
    if (!publishWithSubtopics(object, topic("/props"))) {
      return;
    }
  }

  last_published = props.timestamp;
  next_time = now + mqttPropsInterval;

  Log(DBG, "%spublishProps::success", _LOG_);
}

void MqttAdapter::publishStats(const uint32_t now)
{
  if (!settings.mqtt.publishStatus)
    return;

  static uint32_t next_time = 0;
  if (now < next_time)
    return;

  auto stats = source.stats();
  const uint32_t interval = betterTime(settings.mqtt.publishInterval, mqttStatsInterval);

  if (now - stats.timestamp > interval)
  {
    static uint32_t last_time = 0;
    if (now - last_time < 1000)
      return;

    last_time = now;
    if (!cmd.requestStats()) {
      next_time = now + mqttStatsInterval;
      return;
    }

    Log(INFO, "%spublishStats::backoff::refresh-requested(%d)", _LOG_, now - stats.timestamp);
    next_time = now + mqttStatsInterval;
    return;
  }

  Log(DBG, "%spublishStats(publishFormat=%u)", _LOG_, settings.mqtt.publishFormat);
  String json = ArduMower::Domain::Json::encode(stats);
  if ((settings.mqtt.publishFormat == 1) || (settings.mqtt.publishFormat == 3)) {
    if (!client.publish(topic("/stats").c_str(), json.c_str()))
      return;
  }
  if ((settings.mqtt.publishFormat == 2) || (settings.mqtt.publishFormat == 3)) {
    DynamicJsonDocument doc(1024);
    JsonObject object = doc.to<JsonObject>();
    stats.marshal(object);
    if (!publishWithSubtopics(object, topic("/stats"))) {
      return;
    }
  }
  Log(DBG, "%spublishStats done", _LOG_, settings.mqtt.publishFormat);

  next_time = now + interval;
}

void MqttAdapter::onMqttMessage(String topic, String payload)
{
  Log(DBG, "%sonMqttMessage(topic=%s,payload=%s)", _LOG_, topic.c_str(), payload.c_str());
  if (topic.endsWith("ha/set_fan_speed"))
  {
    if (settings.mqtt.ha)
      ha.onFanSpeedMessage(payload);
    return;
  }

  if (topic.indexOf("/iob/command/") != -1) 
  {
    if (settings.mqtt.iob)
      iob.evaluateMessage(topic, payload);
    return;
  }

  if (payload.startsWith("{"))
  {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    const char *command = doc["command"];
    onMqttMessage(topic, command);
    return;
  }
  else if (payload == "start")
    cmd.start();
  else if (payload == "stop")
    cmd.stop();
  else if (payload == "dock" || payload == "return_to_base")
    cmd.dock();
  else if (payload == "reboot")
    cmd.reboot();
  else if (payload == "shutdown")
    cmd.powerOff();
  else if (payload != "")
    cmd.customCmd(payload);
}

bool MqttAdapter::handleConnection(const uint32_t now)
{
  static uint32_t last_connect_attempt = 0;

  // MQTT already connected
  if (client.loop())
  {
    backoff.reset();
    return true;
  }

  if (!WiFi.isConnected())
  {
    if (_connectFd >= 0) { close(_connectFd); _connectFd = -1; }
    return false;
  }

  // Non-blocking TCP connect in progress: poll with select(0)
  if (_connectFd >= 0)
  {
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(_connectFd, &fdset);
    struct timeval tv = {0, 0};

    int res = select(_connectFd + 1, nullptr, &fdset, nullptr, &tv);

    if (res > 0 && FD_ISSET(_connectFd, &fdset))
    {
      int error = 0;
      socklen_t len = sizeof(error);
      getsockopt(_connectFd, SOL_SOCKET, SO_ERROR, &error, &len);

      if (error == 0)
      {
        int fd = _connectFd;
        _connectFd = -1;

        WiFiClient connected(fd);
        net = connected;

        client.setWill(topic("/online").c_str(), "false");

        if (!client.connect(settings.general.name.c_str(), settings.mqtt.username.c_str(), settings.mqtt.password.c_str(), true))
        {
          Log(ERR, "%scan not connect (MQTT)", _LOG_);
          return false;
        }

        backoff.reset();
        return onMqttConnected();
      }

      close(_connectFd);
      _connectFd = -1;
    }
    else if (res == 0)
    {
      if (now - _connectStart > _connectTimeout)
      {
        close(_connectFd);
        _connectFd = -1;
        Log(ERR, "%sconnect timeout", _LOG_);
      }
      return false;
    }
    else
    {
      close(_connectFd);
      _connectFd = -1;
    }
  }

  if (now - last_connect_attempt < 1000)
    return false;
  last_connect_attempt = now;

  static uint32_t next_time = 0;
  if (now < next_time)
    return false;
  next_time = now + backoff.next();

  Log(INFO, "%sconnect", _LOG_);

  net.stop();

  ArduMower::Util::URL url(settings.mqtt.server);
  int port = url.port();
  if (port == -1) port = 1883;

  static IPAddress cached_addr;
  static uint32_t addr_resolved_at = 0;

  if (addr_resolved_at == 0 || now - addr_resolved_at > 60000)
  {
    IPAddress addr;
    if (!addr.fromString(url.hostname().c_str()))
    {
      if (!WiFi.hostByName(url.hostname().c_str(), addr))
      {
        Log(ERR, "%scan not resolve host", _LOG_);
        return false;
      }
    }
    cached_addr = addr;
    addr_resolved_at = now;
  }

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    Log(ERR, "%scannot create socket", _LOG_);
    return false;
  }

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = (uint32_t)cached_addr;
  sa.sin_port = htons(port);

  int cr = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
  if (cr < 0 && errno != EINPROGRESS)
  {
    Log(ERR, "%sconnect errno=%d", _LOG_, errno);
    close(fd);
    addr_resolved_at = 0;
    return false;
  }

  if (cr == 0)
  {
    WiFiClient connected(fd);
    net = connected;

    client.setWill(topic("/online").c_str(), "false");

    if (!client.connect(settings.general.name.c_str(), settings.mqtt.username.c_str(), settings.mqtt.password.c_str(), true))
    {
      Log(ERR, "%scan not connect (MQTT)", _LOG_);
      return false;
    }

    backoff.reset();
    return onMqttConnected();
  }

  _connectFd = fd;
  _connectStart = now;
  _connectTimeout = 3000;
  return false;

}

bool MqttAdapter::onMqttConnected()
{
  if (!client.subscribe(topic("/command").c_str())) {
    Log(ERR, "%scan not subscribe /command", _LOG_);
    return false;
  }

  if (!client.publish(topic("/online").c_str(), "true")) {
    Log(ERR, "%scan not subscribe /online", _LOG_);
    return false;
  }

  if (settings.mqtt.ha)
  {
    // if (!client.subscribe(topic("/command").c_str()))
    //   return false;

    if (!client.subscribe(topic("/ha/set_fan_speed").c_str()))
      return false;

    ArduMower::Modem::HomeAssistant::DiscoveryDocument disco(settings);
    if (!client.publish(disco.topic().c_str(), disco.toJson(topic("")).c_str()))
      return false;
  }

  if (!iob.subscribeTopics())
    return false;

  Log(DBG, "%sconnect::success", _LOG_);

  return true;
}

bool MqttAdapter::publishTo(const String &topicPostfix, const String &message)
{
  return client.publish(topic(topicPostfix).c_str(), message.c_str());
}

String MqttAdapter::topic(String postfix)
{
  String result = settings.mqtt.prefix;
  result += settings.general.name;
  result += postfix;

  return result;
}


bool MqttAdapter::publishWithSubtopics(JsonObject& data, const String baseTopic) {
  for (JsonPair pair : data) {
    String subTopic = baseTopic + "/" + pair.key().c_str();
    String value;

    // Konvertiere den Wert in einen String, abhängig vom Datentyp
    if (pair.value().is<int>()) {
      value = String(pair.value().as<int>());
    } else if (pair.value().is<float>()) {
      value = String(pair.value().as<float>());
    } else if (pair.value().is<double>()) {
      value = String(pair.value().as<double>());
    } else if (pair.value().is<bool>()) {
      value = String(pair.value().as<bool>());
    } else if (pair.value().is<const char*>()) {
      value = pair.value().as<const char*>();
    } else if (pair.value().is<String>()) {
      value = pair.value().as<String>();
    } else if (pair.value().is<JsonObject>()) {
      // Rekursiver Aufruf für verschachtelte Objekte (optional, siehe Hinweise)
      // publishWithSubtopics(pair.value().as<JsonObject>(), subTopic.c_str());
      // Oder: Serialisiere das Unterobjekt als JSON-String

      JsonObject object = pair.value().as<JsonObject>();
      if (!publishWithSubtopics(object, subTopic)) {
        return false;
      }
      continue;
      
    } else if (pair.value().is<JsonArray>()) {
      // Serialisiere das Array als JSON-String
      String arrayJsonString;
      serializeJson(pair.value(), arrayJsonString);
      if (!client.publish(subTopic.c_str(), arrayJsonString.c_str())) {
        return false;
      }
      continue;

    } else {
      String typeInfo = "unknown";
      if (pair.value().isNull()) typeInfo = "null";
      else if (pair.value().is<signed char>()) typeInfo = "signed char";
      else if (pair.value().is<unsigned char>()) typeInfo = "unsigned char";
      else if (pair.value().is<short>()) typeInfo = "short";
      else if (pair.value().is<unsigned short>()) typeInfo = "unsigned short";
      else if (pair.value().is<long long>()) typeInfo = "long long";
      else if (pair.value().is<unsigned long long>()) typeInfo = "unsigned long long";

      Log(WARN, "%spublishWithSubtopics: unknown datatype for key '%s': %s", _LOG_, pair.key().c_str(), typeInfo.c_str());
      return false;
    }

    if (!client.publish(subTopic.c_str(), value.c_str())) {
      return false;
    }
  }

  return true;
}