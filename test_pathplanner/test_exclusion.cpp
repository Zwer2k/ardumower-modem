#include <pathplanner.h>
#include <cmath>
#include <iostream>
#include <cstdlib>

using namespace ArduMower::Modem::PathPlannerCore;

static bool pointInOrOnPolygon(const Point &p, const Polygon &poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].Y > p.Y) != (poly[j].Y > p.Y) &&
            p.X < (poly[j].X - poly[i].X) * (p.Y - poly[i].Y) / (poly[j].Y - poly[i].Y) + poly[i].X)
            inside = !inside;
    }
    if (inside) return true;
    // boundary check
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx * dx + dy * dy;
        if (len2 < 1e-10) continue;
        double t = std::max(0.0, std::min(1.0, ((p.X - poly[i].X) * dx + (p.Y - poly[i].Y) * dy) / len2));
        double px = poly[i].X + t * dx, py = poly[i].Y + t * dy;
        if ((p.X - px) * (p.X - px) + (p.Y - py) * (p.Y - py) < 1e-6) return true;
    }
    return false;
}

static bool segmentCrossesPolygon(const Point &a, const Point &b, const Polygon &poly) {
    if (poly.size() < 3) return false;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double o1 = (poly[j].X - poly[i].X) * (a.Y - poly[i].Y) - (poly[j].Y - poly[i].Y) * (a.X - poly[i].X);
        double o2 = (poly[j].X - poly[i].X) * (b.Y - poly[i].Y) - (poly[j].Y - poly[i].Y) * (b.X - poly[i].X);
        double o3 = (b.X - a.X) * (poly[i].Y - a.Y) - (b.Y - a.Y) * (poly[i].X - a.X);
        double o4 = (b.X - a.X) * (poly[j].Y - a.Y) - (b.Y - a.Y) * (poly[j].X - a.X);
        if (o1 * o2 < 0 && o3 * o4 <= 0) return true;
    }
    return false;
}

static bool segmentInsideExclusion(const Point &a, const Point &b, const Polygon &exclusion) {
    int samples = std::max(3, (int)(std::hypot(b.X - a.X, b.Y - a.Y) / 0.05));
    if (samples > 50) samples = 50;
    for (int k = 0; k <= samples; k++) {
        double t = (double)k / samples;
        Point p{a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
        if (pointInOrOnPolygon(p, exclusion)) return true;
    }
    return false;
}

int main() {
    Map map;
    map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
    map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};

    Settings settings;
    settings.pattern = 0;          // lines
    settings.width = 0.5f;
    settings.angle = 0;
    settings.distanceToBorder = 0; // no extra inward offset
    settings.borderLaps = 0;
    settings.mowArea = true;
    settings.mowBorderCcw = false;

    Polygon waypoints = calculateWaypoints(map, settings, nullptr);

    std::cout << "Generated " << waypoints.size() << " waypoints" << std::endl;

    if (waypoints.size() < 2) {
        std::cerr << "FAIL: too few waypoints" << std::endl;
        return 1;
    }

    int violations = 0;
    for (size_t i = 1; i < waypoints.size(); i++) {
        if (segmentInsideExclusion(waypoints[i - 1], waypoints[i], map.exclusions[0])) {
            std::cerr << "FAIL: segment " << i - 1 << " ("
                      << waypoints[i - 1].X << "," << waypoints[i - 1].Y << ") -> ("
                      << waypoints[i].X << "," << waypoints[i].Y << ") runs through exclusion"
                      << std::endl;
            violations++;
        }
    }

    if (violations > 0) {
        std::cerr << "FAIL: " << violations << " segment(s) intersect the exclusion" << std::endl;
        return 1;
    }

    std::cout << "PASS: no waypoint segment intersects the exclusion" << std::endl;
    return 0;
}
