#pragma once
#ifdef ENABLE_MAP
#include <vector>
#include "mower_map.h"
#include "domain.h"

namespace ArduMower {
namespace Modem {
namespace PathPlanner {

using Point = ArduMower::Domain::Robot::MapPoint;
using Polygon = std::vector<Point>;

Polygon calculateWaypoints(ArduMower::Domain::Robot::MowerMap &map,
    ArduMower::Domain::Robot::MowSettings &settings,
    const ArduMower::Domain::Robot::State::State *state = nullptr);

} // namespace PathPlanner
} // namespace Modem
} // namespace ArduMower
#endif
