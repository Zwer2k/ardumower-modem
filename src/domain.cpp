#include "domain.h"

using namespace ArduMower::Domain::Robot;

const char * _t_state_batteryVoltage = "battery_voltage";
const char * _t_state_position = "position";
const char * _t_state_target = "target";
const char * _t_state_job = "job";
const char * _t_state_sensor = "sensor";
const char * _t_state_amps = "amps";
const char * _t_state_mapCrc = "map_crc";

const char * _t_position_delta = "delta";
const char * _t_position_solution = "solution";
const char * _t_position_age = "age";
const char * _t_position_accuracy = "accuracy";
const char * _t_position_visibleSatellites = "visible_satellites";
const char * _t_position_visibleSatellitesDgps = "visible_satellites_dgps";
const char * _t_position_mowPointIndex = "mow_point_index";

const char * _t_point_x = "x";
const char * _t_point_y = "y";

const char * _t_desiredState_speed = "speed";
const char * _t_desiredState_mowerMotorEnabled = "mower_motor_enabled";
const char * _t_desiredState_finishAndRestart = "finish_and_restart";
const char * _t_desiredState_op = "op";
const char * _t_desiredState_fixTimeout = "fix_timeout";

#define same(other, prop) (prop == other.prop)

bool Properties::operator==(const Properties &other)
{
  return same(other, timestamp) && same(other, firmware) && same(other, version);
}

bool Stats::Stats::operator==(const Stats &other)
{
  return same(other, timestamp) && same(other, durations) && same(other, recoveries) && same(other, obstacles) && same(other, mowDistanceTraveled) && same(other, mowMaxDgpsAge) && same(other, tempMin) && same(other, tempMax) && same(other, gpsChecksumErrors) && same(other, dgpsChecksumErrors) && same(other, maxMotorControlCycleTime) && same(other, serialBufferSize) && same(other, freeMemory) && same(other, resetCause) && same(other, gpsJumps);
}

bool Stats::Durations::operator==(const Durations &other)
{
  return same(other, idle) && same(other, charge) && same(other, mow) && same(other, mowFloat) && same(other, mowFix) && same(other, mowInvalid);
}

bool Stats::Recoveries::operator==(const Recoveries &other)
{
  return same(other, mowFloatToFix) && same(other, imu) && same(other, mowInvalid);
}

bool Stats::Obstacles::operator==(const Obstacles &other)
{
  return same(other, count) && same(other, sonar) && same(other, bumper) && same(other, gpsMotionLow);
}

const char* State::State::jobDesc[State::State::jobDescLen] = { "idle", "mow", "charge", "error", "dock" };
const char* State::State::posSolutionDesc[State::State::posSolutionDescLen] = { "invalid", "float", "fix" };

bool State::State::operator==(const State &other)
{
  return same(other, timestamp) && same(other, batteryVoltage) && same(other, position) && same(other, target) && same(other, job) && same(other, sensor) && same(other, amps) && same(other, mapCrc) &&
    same(other, temperature) && same(other, chargingMah) && same(other, motorMowMah) && same(other, motorLeftMah) && same(other, motorRightMah);
}

bool State::Position::operator==(const Position &other)
{
  return Point::operator==(other) && same(other, delta) && same(other, solution) && same(other, age) && same(other, accuracy) && same(other, visibleSatellites) && same(other, visibleSatellitesDgps) && same(other, mowPointIndex);
}

bool State::Point::operator==(const Point &other)
{
  return same(other, x) && same(other, y);
}

void State::State::marshal(const JsonObject &o) const
{
  o[_t_state_batteryVoltage] = serialized(String(batteryVoltage, 2));
  o.createNestedObject(_t_state_position);
  position.marshal(o[_t_state_position]);
  o.createNestedObject(_t_state_target);
  target.marshal(o[_t_state_target]);
  o[_t_state_job] = job;
  o[_t_state_sensor] = sensor;
  o[_t_state_amps] = serialized(String(amps, 2));
  o[_t_state_mapCrc] = mapCrc;
}

void State::Position::marshal(const JsonObject &o) const
{
  o[_t_position_delta] = delta;
  o[_t_position_solution] = solution;
  o[_t_position_age] = age;
  o[_t_position_accuracy] = accuracy;
  o[_t_position_visibleSatellites] = visibleSatellites;
  o[_t_position_visibleSatellitesDgps] = visibleSatellitesDgps;
  o[_t_position_mowPointIndex] = mowPointIndex;
  o[_t_point_x] = serialized(String(x, 8));
  o[_t_point_y] = serialized(String(y, 8));
}

void State::Point::marshal(const JsonObject &o) const
{
  o[_t_point_x] = serialized(String(x, 8));
  o[_t_point_y] = serialized(String(y, 8));
}


void DesiredState::marshal(const JsonObject &o) const
{
  o[_t_desiredState_speed] = speed;
  o[_t_desiredState_mowerMotorEnabled] = mowerMotorEnabled;
  o[_t_desiredState_finishAndRestart] = finishAndRestart;
  o[_t_desiredState_op] = op;
  o[_t_desiredState_fixTimeout] = fixTimeout;
}
