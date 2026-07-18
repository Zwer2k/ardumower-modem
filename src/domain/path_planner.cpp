#ifdef ENABLE_MAP
#include "path_planner.h"
#include "pathplanner.h"
#include "mower_map.h"
#include "domain.h"
#include "log.h"

#define _LOG_ "PathPlanner::"

namespace ArduMower {
namespace Modem {
namespace PathPlanner {

namespace PPC = ArduMower::Modem::PathPlannerCore;

static PPC::Point toPlannerPoint(const ArduMower::Domain::Robot::MapPoint &p) {
    return PPC::Point{p.X, p.Y};
}

static PPC::Point toPlannerPoint(const ArduMower::Domain::Robot::State::Point &p) {
    return PPC::Point{p.x, p.y};
}

static ArduMower::Domain::Robot::MapPoint toDomainPoint(const PPC::Point &p) {
    return ArduMower::Domain::Robot::MapPoint{p.X, p.Y};
}

static PPC::Map toPlannerMap(ArduMower::Domain::Robot::MowerMap &map) {
    PPC::Map pm;
    pm.timestamp = map.timestamp;
    pm.rotation = map.rotation;

    pm.perimeter.reserve(map.perimeter.size());
    for (const auto &p : map.perimeter) pm.perimeter.push_back(toPlannerPoint(p));

    pm.exclusions.reserve(map.exclusions.size());
    for (const auto &ex : map.exclusions) {
        std::vector<PPC::Point> poly;
        poly.reserve(ex.size());
        for (const auto &p : ex) poly.push_back(toPlannerPoint(p));
        pm.exclusions.push_back(std::move(poly));
    }

    pm.dockpoints.reserve(map.dockpoints.size());
    for (const auto &p : map.dockpoints) pm.dockpoints.push_back(toPlannerPoint(p));

    return pm;
}

static PPC::Settings toPlannerSettings(ArduMower::Domain::Robot::MowSettings &settings) {
    PPC::Settings ps;
    ps.timestamp = settings.timestamp;
    ps.pattern = settings.pattern;
    ps.width = settings.width;
    ps.angle = settings.angle;
    ps.distanceToBorder = settings.distanceToBorder;
    ps.borderLaps = settings.borderLaps;
    ps.mowArea = settings.mowArea;
    ps.mowExclusionBorder = settings.mowExclusionBorder;
    ps.mowBorderCcw = settings.mowBorderCcw;
    return ps;
}

static PPC::State toPlannerState(const ArduMower::Domain::Robot::State::State *state) {
    PPC::State ps;
    if (state != nullptr) {
        ps.job = state->job;
        ps.position.x = state->position.x;
        ps.position.y = state->position.y;
        ps.position.solution = state->position.solution;
    }
    return ps;
}

Polygon calculateWaypoints(ArduMower::Domain::Robot::MowerMap &map,
    ArduMower::Domain::Robot::MowSettings &settings,
    const ArduMower::Domain::Robot::State::State *state)
{
    PPC::Map pm = toPlannerMap(map);
    PPC::Settings ps = toPlannerSettings(settings);
    const PPC::State pstate = toPlannerState(state);

    PPC::Polygon route = PPC::calculateWaypoints(pm, ps, state ? &pstate : nullptr);

    Polygon result;
    result.reserve(route.size());
    for (const auto &p : route) result.push_back(toDomainPoint(p));
    return result;
}

} // namespace PathPlanner
} // namespace Modem
} // namespace ArduMower
#endif
