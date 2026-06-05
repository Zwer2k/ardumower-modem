#pragma once

#include <Arduino.h>
#include <inttypes.h>
#include <ArduinoJson.h>
#include "mower_map.h"

namespace ArduMower
{
  namespace Domain
  {
    namespace Robot
    {
      class Properties
      {
      public:
        uint32_t timestamp = 0;
        String firmware;
        String version;

        bool operator==(const Properties &other);
        bool operator!=(const Properties &other) { return !(*this == other); }
        void marshal(const JsonObject &o) const;
      };

      namespace Stats
      {
        class Durations
        {
        public:
          uint32_t idle, charge, mow, mowFloat, mowFix, mowInvalid;

          bool operator==(const Durations &other);
          bool operator!=(const Durations &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };

        class Recoveries
        {
        public:
          uint32_t mowFloatToFix;
          uint32_t imu;
          uint32_t mowInvalid;

          bool operator==(const Recoveries &other);
          bool operator!=(const Recoveries &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };

        class Obstacles
        {
        public:
          uint32_t count;
          uint32_t sonar;
          uint32_t bumper;
          uint32_t gpsMotionLow;

          bool operator==(const Obstacles &other);
          bool operator!=(const Obstacles &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };

        class Stats
        {
        public:
          uint32_t timestamp;
          Durations durations;
          Recoveries recoveries;
          Obstacles obstacles;
          float mowDistanceTraveled;
          float mowMaxDgpsAge;
          float tempMin, tempMax;
          uint32_t gpsChecksumErrors;
          uint32_t dgpsChecksumErrors;
          float maxMotorControlCycleTime;
          uint32_t serialBufferSize;
          uint32_t freeMemory;
          int resetCause;

          uint32_t gpsJumps;

          Stats() : timestamp(0), mowDistanceTraveled(0), mowMaxDgpsAge(0), tempMin(0), tempMax(0), gpsChecksumErrors(0), dgpsChecksumErrors(0), maxMotorControlCycleTime(0), serialBufferSize(0), freeMemory(0), resetCause(0), gpsJumps(0) {}

          bool operator==(const Stats &other);
          bool operator!=(const Stats &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };
      }

      namespace State
      {
        class Point
        {
        public:
          float x, y;

          Point()
              : x(0), y(0){};

          bool operator==(const Point &other);
          bool operator!=(const Point &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };

        class Position : public Point
        {
        public:
          float delta;
          int solution;
          float age;
          float accuracy;
          int visibleSatellites, visibleSatellitesDgps;
          int mowPointIndex;

          Position()
              : delta(0),
                solution(0), age(0), accuracy(0), visibleSatellites(0), visibleSatellitesDgps(0),
                mowPointIndex(0){};

          bool operator==(const Position &other);
          bool operator!=(const Position &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };

        class State
        {
        public:
          uint32_t timestamp;
          float batteryVoltage;
          Position position;
          Point target;
          int job;
          int sensor;
          float amps;
          int mapCrc;
          float temperature;
          float chargingMah;
          float motorMowMah;
          float motorLeftMah;
          float motorRightMah;

          static const byte jobDescLen = 5;
          static const char* jobDesc[jobDescLen];

          static const byte posSolutionDescLen = 3;
          static const char* posSolutionDesc[posSolutionDescLen];


          State()
              : timestamp(0), batteryVoltage(0), job(0), sensor(0), amps(0), mapCrc(0), temperature(0), chargingMah(0), motorMowMah(0), motorLeftMah(0), motorRightMah(0)
          {
          }

          bool operator==(const State &other);
          bool operator!=(const State &other) { return !(*this == other); }
          void marshal(const JsonObject &o) const;
        };
      }

      class DesiredState
      {
      public:
        uint32_t timestamp;
        float speed;
        bool mowerMotorEnabled;
        bool finishAndRestart;
        int op;
        int fixTimeout;

        DesiredState() : timestamp(1), speed(0), mowerMotorEnabled(false), finishAndRestart(false), op(-1), fixTimeout(-1){};
        void marshal(const JsonObject &o) const;
      };

      class SensorSummary
      {
      public:
        uint32_t timestamp;
        float sonarLeft;
        float sonarCenter;
        float sonarRight;
        bool sonarObstacle;
        bool sonarNearObstacle;
        bool bumperLeft;
        bool bumperRight;
        bool bumperObstacle;
        bool bumperNearObstacle;
        bool lidarObstacle;
        bool lidarNearObstacle;
        bool liftTriggered;
        bool rainTriggered;

        SensorSummary()
            : timestamp(0), sonarLeft(0), sonarCenter(0), sonarRight(0),
              sonarObstacle(false), sonarNearObstacle(false),
              bumperLeft(false), bumperRight(false),
              bumperObstacle(false), bumperNearObstacle(false),
              lidarObstacle(false), lidarNearObstacle(false),
              liftTriggered(false), rainTriggered(false)
        {
        }

        bool operator==(const SensorSummary &other);
        bool operator!=(const SensorSummary &other) { return !(*this == other); }
        void marshal(const JsonObject &o) const;
      };

      class GpsSatellite
      {
      public:
        uint8_t gnssId;
        uint8_t svId;
        uint8_t sigId;
        uint8_t cno;
        uint8_t qualityInd;
        bool prUsed;
        bool crCorrUsed;
        float prRes;
        int8_t elevation;
        int8_t azimuth;

        GpsSatellite()
            : gnssId(0), svId(0), sigId(0), cno(0), qualityInd(0),
              prUsed(false), crCorrUsed(false), prRes(0), elevation(0), azimuth(0)
        {
        }

        void marshal(const JsonObject &o) const;
      };

      class GpsDetails
      {
      public:
        uint32_t timestamp;
        int numSV;
        int numSVdgps;
        int solution;
        float hAccuracy;
        float vAccuracy;
        uint32_t dgpsAge;
        std::vector<GpsSatellite> satellites;

        GpsDetails()
            : timestamp(0), numSV(0), numSVdgps(0), solution(0),
              hAccuracy(0), vAccuracy(0), dgpsAge(0)
        {
        }

        bool operator==(const GpsDetails &other);
        bool operator!=(const GpsDetails &other) { return !(*this == other); }
        void marshal(const JsonObject &o) const;
      };

      class UbxResponse
      {
      public:
        uint32_t timestamp;
        String hexData;

        UbxResponse() : timestamp(0) {}

        bool operator==(const UbxResponse &other) {
          return timestamp == other.timestamp && hexData == other.hexData;
        }
        bool operator!=(const UbxResponse &other) { return !(*this == other); }
        void marshal(const JsonObject &o) const;
      };

      class StateSource
      {
      public:
        virtual ArduMower::Domain::Robot::State::State state() = 0;
        virtual ArduMower::Domain::Robot::Stats::Stats stats() = 0;
        virtual ArduMower::Domain::Robot::Properties props() = 0;
        virtual DesiredState desiredState() = 0;

        virtual ArduMower::Domain::Robot::State::State *stateP() = 0;
        virtual ArduMower::Domain::Robot::Stats::Stats *statsP() = 0;
        virtual ArduMower::Domain::Robot::Properties *propsP() = 0;
        virtual DesiredState *desiredStateP() = 0;
        virtual SensorSummary sensorSummary() = 0;
        virtual SensorSummary *sensorSummaryP() = 0;
        virtual GpsDetails gpsDetails() = 0;
        virtual GpsDetails *gpsDetailsP() = 0;
        virtual UbxResponse ubxResponse() = 0;
        virtual UbxResponse *ubxResponseP() = 0;

        virtual ArduMower::Domain::Robot::MowerMap mowerMap() = 0;
        virtual void setMap(const ArduMower::Domain::Robot::MowerMap &map) = 0;
      };

      class CommandExecutor
      {
      public:

        virtual bool changeSpeed(float speed) = 0;
        virtual bool dock() = 0;
        virtual bool finishAndRestartEnabled(bool enabled) = 0;
        virtual bool mowerEnabled(bool enabled) = 0;
        virtual bool setFixTimeout(int timeout) = 0;
        virtual bool setWaypoint(float waypoint) = 0;
        virtual bool skipWaypoint() = 0;
        virtual bool start() = 0;
        virtual bool stop() = 0;
        virtual bool sonarEnabled(bool enabled) = 0;

        virtual bool requestVersion() = 0;
        virtual bool requestStatus() = 0;
        virtual bool requestStats() = 0;
        virtual bool requestSensorSummary() = 0;
        virtual bool requestGpsDetails() = 0;
        virtual bool sendUbx(const String &hexCmd) = 0;

        virtual bool manualDrive(float linear, float angular) = 0;
        virtual bool navigateTo(float x, float y) = 0;

        virtual bool reboot() = 0;
        virtual bool rebootGPS() = 0;
        virtual bool powerOff() = 0;

        virtual bool uploadMapToMower() = 0;
        virtual bool customCmd(String cmd) = 0;
      };
    }
  }
}
