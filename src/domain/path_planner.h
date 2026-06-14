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

double distance(const Point &a, const Point &b);
double crossProduct(const Point &a, const Point &b, const Point &c);
bool pointInPolygon(const Point &p, const Polygon &poly);
Point rotatePoint(const Point &p, double angleDeg);
Polygon rotatePolygon(const Polygon &poly, double angleDeg);
std::vector<Polygon> rotatePolygons(const std::vector<Polygon> &polys, double angleDeg);
void boundingBox(const Polygon &poly, double &minX, double &minY, double &maxX, double &maxY);
double polygonArea(const Polygon &poly);
bool isClockwise(const Polygon &poly);
size_t nearestPointIndex(const Point &p, const Polygon &poly);

struct Intersection {
    double x;
    int edgeIndex;
};
std::vector<Intersection> intersectRayWithPolygon(double y, const Polygon &poly);

std::vector<Polygon> offsetPolygonInward(const Polygon &poly, double distance);

Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, const Point &startNear);
Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear, double width);

void sortSolutionPolygonsByDistance(std::vector<Polygon> &solution, const Point &startPt);
void connectPolysUsingPathFinding(Polygon &waypoints, const std::vector<Polygon> &polys,
    const Polygon &perimeter);

Polygon calculateWaypoints(ArduMower::Domain::Robot::MowerMap &map,
    ArduMower::Domain::Robot::MowSettings &settings);

        }
    }
}
#endif
