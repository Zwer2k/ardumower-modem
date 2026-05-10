#include "domain.h"

using namespace ArduMower::Domain::Robot;

const char * _t_props_firmware = "firmware";
const char * _t_props_version = "version";

const char * _t_duration_idle = "idle";
const char * _t_duration_charge = "charge";
const char * _t_duration_mow = "mow";
const char * _t_duration_mow_invalid = "mow_invalid";
const char * _t_duration_mow_float = "mow_float";
const char * _t_duration_mow_fix = "mow_fix";

const char * _t_recoveries_invalid_recoveries = "invalid_recoveries";
const char * _t_recoveries_float_recoveries = "float_recoveries";
const char * _t_recoveries_imu_triggered = "imu_triggered";

const char * _t_obstacles_gps_motion_timeout = "gps_motion_timeout";
const char * _t_obstacles_sonar_triggered = "sonar_triggered";
const char * _t_obstacles_bumper_triggered = "bumper_triggered";
const char * _t_obstacles_obstacles = "obstacles";

const char * _t_state_durations = "durations";
const char * _t_stats_mow_traveled = "mow_traveled";
const char * _t_stats_gps_chk_sum_errors = "gps_chk_sum_errors";
const char * _t_stats_dgps_chk_sum_errors = "dgps_chk_sum_errors";
const char * _t_stats_recoveries = "recoveries";
const char * _t_stats_obstacles = "obstacles";
const char * _t_stats_gps_jumps = "gps_jumps";
const char * _t_stats_max_cycle = "max_cycle";
const char * _t_stats_max_dpgs_age = "max_dpgs_age";
const char * _t_stats_serial_buffer_size = "serial_buffer_size";
const char * _t_stats_free_memory = "free_memory";
const char * _t_stats_reset_cause = "reset_cause";
const char * _t_stats_temp_min = "temp_min";
const char * _t_stats_temp_max = "temp_max";

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

void Stats::Durations::marshal(const JsonObject &o) const
{
  o[_t_duration_idle] = idle;
  o[_t_duration_charge] = charge;
  o[_t_duration_mow] = mow;
  o[_t_duration_mow_invalid] = mowInvalid;
  o[_t_duration_mow_float] = mowFloat;
  o[_t_duration_mow_fix] = mowFix;
}

void Stats::Recoveries::marshal(const JsonObject &o) const
{
  o[_t_recoveries_invalid_recoveries] = mowInvalid;
  o[_t_recoveries_float_recoveries] = mowFloatToFix;
  o[_t_recoveries_imu_triggered] = imu;
}

void Stats::Obstacles::marshal(const JsonObject &o) const
{
  o[_t_obstacles_gps_motion_timeout] = gpsMotionLow;
  o[_t_obstacles_sonar_triggered] = sonar;
  o[_t_obstacles_bumper_triggered] = bumper;
  o[_t_obstacles_obstacles] = count;
}

void Stats::Stats::marshal(const JsonObject &o) const
{
  o.createNestedObject(_t_state_durations);
  durations.marshal(o[_t_state_durations]);
  o[_t_stats_mow_traveled] = mowDistanceTraveled;
  o[_t_stats_gps_chk_sum_errors] = gpsChecksumErrors;
  o[_t_stats_dgps_chk_sum_errors] = dgpsChecksumErrors;
  o.createNestedObject(_t_stats_recoveries);
  recoveries.marshal(o[_t_stats_recoveries]);
  o.createNestedObject(_t_stats_obstacles);
  obstacles.marshal(o[_t_stats_obstacles]);
  o[_t_stats_gps_jumps] = gpsJumps;
  o[_t_stats_max_cycle] = maxMotorControlCycleTime;
  o[_t_stats_max_dpgs_age] = mowMaxDgpsAge;
  o[_t_stats_serial_buffer_size] = serialBufferSize;
  o[_t_stats_free_memory] = freeMemory;
  o[_t_stats_reset_cause] = resetCause;  
  o[_t_stats_temp_min] = tempMin;
  o[_t_stats_temp_max] = tempMax;
}

void Properties::marshal(const JsonObject &o) const
{
  o[_t_props_firmware] = firmware;
  o[_t_props_version] = version;
}

void State::State::marshal(const JsonObject &o) const
{
  o[_t_state_batteryVoltage] = batteryVoltage;
  o.createNestedObject(_t_state_position);
  position.marshal(o[_t_state_position]);
  o.createNestedObject(_t_state_target);
  target.marshal(o[_t_state_target]);
  o[_t_state_job] = job;
  o[_t_state_sensor] = sensor;
  o[_t_state_amps] = amps;
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
  o[_t_point_x] = x;
  o[_t_point_y] = y;
}

void State::Point::marshal(const JsonObject &o) const
{
  o[_t_point_x] = x;
  o[_t_point_y] = y;
}


void DesiredState::marshal(const JsonObject &o) const
{
  o[_t_desiredState_speed] = speed;
  o[_t_desiredState_mowerMotorEnabled] = mowerMotorEnabled;
  o[_t_desiredState_finishAndRestart] = finishAndRestart;
  o[_t_desiredState_op] = op;
  o[_t_desiredState_fixTimeout] = fixTimeout;
}
