#pragma once
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>
#include <string>

// Stub Arduino types
using String = std::string;

enum LogLevel {
  COMM = 32,
  DBG  = 16,
  INFO = 8,
  WARN = 4,
  ERR  = 2,
  CRIT = 1
};

#define Log(X, ...) // no-op for tests

namespace ArduMower {
namespace Domain {
namespace Robot {

struct MapPoint {
    double X;
    double Y;
};

struct MowerMap {
    uint32_t timestamp;
    std::vector<MapPoint> perimeter;
    std::vector<std::vector<MapPoint>> exclusions;
    std::vector<MapPoint> waypoints;
    std::vector<MapPoint> dockpoints;
};

struct MowSettings {
    uint32_t timestamp;
    int pattern;
    float width;
    int angle;
    int distanceToBorder;
    int borderLaps;
    bool mowArea;
    bool mowExclusionBorder;
    bool mowBorderCcw;

    MowSettings()
        : timestamp(0), pattern(0), width(0.3f), angle(0),
          distanceToBorder(0), borderLaps(0),
          mowArea(true), mowExclusionBorder(false), mowBorderCcw(false) {}
};

} // namespace Robot
} // namespace Domain
} // namespace ArduMower
