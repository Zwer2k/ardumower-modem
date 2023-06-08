#include "json.h"
#include <ArduinoJson.h>

using namespace ArduMower::Domain::Robot;
using namespace ArduMower::Domain;

static void encodeInto(JsonObject& doc, State::Point &p)
{
  doc["x"] = p.x;
  doc["y"] = p.y;
}

static void encodeInto(JsonObject& doc, State::Position &p)
{
  doc["x"] = p.x;
  doc["y"] = p.y;
  doc["delta"] = p.delta;
  doc["solution"] = p.solution;
  doc["age"] = p.age;
  doc["accuracy"] = p.accuracy;
  doc["visible_satellites"] = p.visibleSatellites;
  doc["visible_satellites_dgps"] = p.visibleSatellitesDgps;
  doc["mow_point_index"] = p.mowPointIndex;
}

String Json::encode(Properties &p)
{
  String result;
  DynamicJsonDocument doc(1024);

  doc["firmware"] = p.firmware;
  doc["version"] = p.version;

  serializeJson(doc, result);

  return result;
}

String Json::encode(State::State &s)
{
  String result;
  DynamicJsonDocument doc(1024);
  s.marshal(doc.to<JsonObject>());
  serializeJson(doc, result);
  
  return result;
}

String Json::encode(Stats::Stats &stats)
{
  String result;
  DynamicJsonDocument doc(1024);

  doc["duration_idle"] = stats.durations.idle;
  doc["duration_charge"] = stats.durations.charge;
  doc["duration_mow"] = stats.durations.mow;
  doc["duration_mow_invalid"] = stats.durations.mowInvalid;
  doc["duration_mow_float"] = stats.durations.mowFloat;
  doc["duration_mow_fix"] = stats.durations.mowFix;

  doc["distance_mow_traveled"] = stats.mowDistanceTraveled;
  
  doc["counter_gps_chk_sum_errors"] = stats.gpsChecksumErrors;
  doc["counter_dgps_chk_sum_errors"] = stats.dgpsChecksumErrors;
  doc["counter_invalid_recoveries"] = stats.recoveries.mowInvalid;
  doc["counter_float_recoveries"] = stats.recoveries.mowFloatToFix;
  doc["counter_gps_jumps"] = stats.gpsJumps;
  doc["counter_gps_motion_timeout"] = stats.obstacles.gpsMotionLow;
  doc["counter_imu_triggered"] = stats.recoveries.imu;
  doc["counter_sonar_triggered"] = stats.obstacles.sonar;
  doc["counter_bumper_triggered"] = stats.obstacles.bumper;
  doc["counter_obstacles"] = stats.obstacles.count;
  
  doc["time_max_cycle"] = stats.maxMotorControlCycleTime;
  doc["time_max_dpgs_age"] = stats.mowMaxDgpsAge;
  
  doc["serial_buffer_size"] = stats.serialBufferSize;
  doc["free_memory"] = stats.freeMemory;
  doc["reset_cause"] = stats.resetCause;
  
  doc["temp_min"] = stats.tempMin;
  doc["temp_max"] = stats.tempMax;

  serializeJson(doc, result);
  
  return result;
}

String Json::encode(DesiredState &d)
{
  String result;
  DynamicJsonDocument doc(1024);
  d.marshal(doc.to<JsonObject>());
  serializeJson(doc, result);
  
  return result;
}