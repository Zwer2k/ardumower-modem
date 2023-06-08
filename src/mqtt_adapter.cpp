#include "mqtt_adapter.h"
#include "mqtt_ha.h"
#include "mqtt_iobroker.h"
#include "json.h"
#include "log.h"
#include "url.h"
#include <ArduinoJson.h>

using namespace ArduMower::Modem;

#define mqttStatusInterval 5000
#define mqttStatsInterval 60000
#define mqttPropsInterval 86400000

void MqttAdapter::begin()
{
  if (!settings.mqtt.enabled)
    return;

  Log(INFO, "MqttAdapter::begin");
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

    Log(DBG, "MqttAdapter::publishState::refresh-requested(%d)", now - state.timestamp);
    return;
  }

  if (state.timestamp == 0)
    return;

  String json = ArduMower::Domain::Json::encode(state);
  Log(DBG, "MqttAdapter::publishState");
  if (!client.publish(topic("/state").c_str(), json.c_str()))
    return;
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
  Log(DBG, "MqttAdapter::publishProps");
  if (!client.publish(topic("/props").c_str(), json.c_str()))
    return;

  last_published = props.timestamp;
  next_time = now + mqttPropsInterval;

  Log(DBG, "MqttAdapter::publishProps::success");
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

    Log(DBG, "MqttAdapter::publishStats::backoff::refresh-requested(%d)", now - stats.timestamp);
    return;
  }

  String json = ArduMower::Domain::Json::encode(stats);
  if (!client.publish(topic("/stats").c_str(), json.c_str()))
    return;

  next_time = now + interval;
}

void MqttAdapter::onMqttMessage(String topic, String payload)
{
  Log(DBG, "MqttAdapter::onMqttMessage(topic=%s,payload=%s)", topic.c_str(), payload.c_str());
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
  static uint32_t last_connected = 0;
  static uint32_t first_disconnected = 0;
  if (client.loop())
  {
    last_connected = now;
    backoff.reset();
    return true;
  }

  if (first_disconnected < last_connected)
  {
    first_disconnected = now;
  }

  if (now - first_disconnected < 1000)
    return false;

  static uint32_t next_time = 0;
  if (now < next_time)
    return false;
  next_time = now + backoff.next();

  Log(INFO, "MqttAdapter::connect");

  client.setWill(topic("/online").c_str(), "false");

  if (!client.connect(settings.general.name.c_str(), settings.mqtt.username.c_str(), settings.mqtt.password.c_str())) {
    Log(ERR, "MqttAdapter::can not connect");
    return false;
  }

  if (!client.subscribe(topic("/command").c_str())) {
    Log(ERR, "MqttAdapter::can not subscribe /command");
    return false;
  }

  if (!client.publish(topic("/online").c_str(), "true")) {
    Log(ERR, "MqttAdapter::can not subscribe /online");
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

  // ############## Init IOBroker Variables ##############
  // if (!iob.createIOBrokerDataPoints())
  //   return false;

  if (!iob.subscribeTopics())
    return false;

  Log(DBG, "MqttAdapter::connect::success");

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
