#include "test_stub.h"
#include <iostream>
#include <vector>
#include <cmath>

namespace ArduMower {
namespace Modem {
namespace PathPlanner {
using Point = ArduMower::Domain::Robot::MapPoint;
using Polygon = std::vector<Point>;
Polygon offsetPolygonInward(const Polygon &poly, double distance);
double polygonArea(const Polygon &poly);
bool isClockwise(const Polygon &poly);
Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg);
Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width);
Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear, double width);
} // namespace PathPlanner
} // namespace Modem
} // namespace ArduMower

using namespace ArduMower::Modem::PathPlanner;

int main() {
    bool ok = true;

    // Rectangle offset
    Polygon rect = {{0,0}, {10,0}, {10,5}, {0,5}};
    Polygon offset = offsetPolygonInward(rect, 1.0);
    ok &= (offset.size() == 4);

    // L-Shape offset
    Polygon lshape = {{0,0}, {10,0}, {10,3}, {5,3}, {5,10}, {0,10}};
    offset = offsetPolygonInward(lshape, 1.0);
    ok &= (offset.size() >= 3 && polygonArea(offset) > 0);

    // Lines pattern
    Polygon lines = calculateLinesPattern(rect, rect, 2.0, 0.0);
    ok &= (lines.size() >= 4);

    // Rings pattern - 10x10 square
    Polygon sq = {{0,0}, {10,0}, {10,10}, {0,10}};
    Polygon rings = calculateRingsPattern(sq, sq, 0.3);
    ok &= (rings.size() >= 4);

    // Rings pattern - small 3x3 square with width=1.0 (should reach center)
    Polygon small = {{0,0}, {3,0}, {3,3}, {0,3}};
    rings = calculateRingsPattern(small, small, 1.0);
    ok &= (rings.size() >= 5);
    bool hasCenter = false;
    for (auto &p : rings) {
        if (std::abs(p.X - 1.5) < 0.01 && std::abs(p.Y - 1.5) < 0.01) hasCenter = true;
    }
    ok &= hasCenter;

    // Border laps
    Polygon border = addBorderLaps(sq, 3, false, {0,0}, 1.0);
    ok &= (border.size() >= 4);

    std::cout << (ok ? "All tests PASSED" : "Some tests FAILED") << "\n";
    return ok ? 0 : 1;
}
