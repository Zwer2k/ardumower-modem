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
double distance(const Point &a, const Point &b);
Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow, double width);
} // namespace PathPlanner
} // namespace Modem
} // namespace ArduMower

using namespace ArduMower::Modem::PathPlanner;

int main() {
    // 10x10 square with 0.3m width
    Polygon rect = {{0,0}, {10,0}, {10,10}, {0,10}};
    std::cout << "=== 10x10 Square, width=0.3 ===\n";
    Polygon rings = calculateRingsPattern(rect, rect, 0.3);
    std::cout << "Total points: " << rings.size() << "\n";
    
    // Count rings by looking for repeated points (ring closures)
    int ringCount = 0;
    for (size_t i = 1; i < rings.size(); i++) {
        if (distance(rings[i], rings[0]) < 0.01 && i > 2) {
            ringCount++;
        }
    }
    std::cout << "Approximate rings: " << ringCount << "\n";
    
    // Show each ring
    std::cout << "\nAll points:\n";
    for (auto &p : rings) std::cout << "  (" << p.X << ", " << p.Y << ")\n";
    
    return 0;
}
