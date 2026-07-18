#pragma once

#include <vector>
#include <cstdint>

namespace ArduMower {
namespace Modem {
namespace PathPlannerCore {

struct Point {
    double X;
    double Y;
};

using Polygon = std::vector<Point>;

struct Map {
    uint32_t timestamp;
    std::vector<Point> perimeter;
    std::vector<std::vector<Point>> exclusions;
    std::vector<Point> waypoints;
    std::vector<Point> dockpoints;
    double rotation = 0.0;
};

struct Settings {
    uint32_t timestamp;
    int pattern;
    float width;
    int angle;
    int distanceToBorder;
    int borderLaps;
    bool mowArea;
    bool mowExclusionBorder;
    bool mowBorderCcw;

    Settings()
        : timestamp(0), pattern(0), width(0.3f), angle(0),
          distanceToBorder(0), borderLaps(0),
          mowArea(true), mowExclusionBorder(false), mowBorderCcw(false) {}
};

struct Position {
    double x;
    double y;
    int solution;
};

struct State {
    int job;
    Position position;
};

// Core geometry helpers
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
    const Polygon &perimeter, const std::vector<Polygon> &areasToMow,
    const std::vector<Polygon> &holes = {});
std::vector<Polygon> clipSegmentsAgainstHoles(const std::vector<Polygon> &segments,
    const std::vector<Polygon> &holes);

Polygon calculateWaypoints(Map &map, Settings &settings, const State *state = nullptr);

} // namespace PathPlannerCore
} // namespace Modem
} // namespace ArduMower
