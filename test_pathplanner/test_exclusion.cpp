#include <pathplanner.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstdlib>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>

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

static bool pointOnBoundary(const Point &p, const Polygon &poly, double tolSq = 1e-6) {
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx * dx + dy * dy;
        if (len2 < 1e-10) continue;
        double t = std::max(0.0, std::min(1.0, ((p.X - poly[i].X) * dx + (p.Y - poly[i].Y) * dy) / len2));
        double px = poly[i].X + t * dx, py = poly[i].Y + t * dy;
        if ((p.X - px) * (p.X - px) + (p.Y - py) * (p.Y - py) < tolSq) return true;
    }
    return false;
}

static bool pointInsideExclusion(const Point &p, const Polygon &exclusion) {
    if (exclusion.size() < 3) return false;
    // Points on the exclusion boundary are not considered "inside" for the
    // purpose of violation detection. This allows mowing along the border when
    // the user explicitly requested it (mowExclusionBorder) while still
    // flagging segments that cross into the interior of the exclusion.
    if (pointOnBoundary(p, exclusion, 1e-6)) return false;
    bool inside = false;
    for (size_t i = 0, j = exclusion.size() - 1; i < exclusion.size(); j = i++) {
        if ((exclusion[i].Y > p.Y) != (exclusion[j].Y > p.Y) &&
            p.X < (exclusion[j].X - exclusion[i].X) * (p.Y - exclusion[i].Y) / (exclusion[j].Y - exclusion[i].Y) + exclusion[i].X)
            inside = !inside;
    }
    return inside;
}

static bool segmentInsideExclusion(const Point &a, const Point &b, const Polygon &exclusion, const Polygon &perimeter) {
    int samples = std::max(3, (int)(std::hypot(b.X - a.X, b.Y - a.Y) / 0.05));
    if (samples > 50) samples = 50;
    for (int k = 0; k <= samples; k++) {
        double t = (double)k / samples;
        Point p{a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
        // Only skip points that lie exactly on the outer perimeter boundary.
        // Points inside the perimeter but inside an exclusion must still be counted.
        if (pointOnBoundary(p, perimeter, 1e-6) && !pointInsideExclusion(p, exclusion)) continue;
        if (pointInsideExclusion(p, exclusion)) return true;
    }
    return false;
}

static double pointToPolygonDistance(const Point &p, const Polygon &poly) {
    if (poly.size() < 3) return 0.0;
    double best = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx * dx + dy * dy;
        if (len2 < 1e-10) continue;
        double t = std::max(0.0, std::min(1.0, ((p.X - poly[i].X) * dx + (p.Y - poly[i].Y) * dy) / len2));
        double px = poly[i].X + t * dx, py = poly[i].Y + t * dy;
        double d = std::hypot(p.X - px, p.Y - py);
        if (d < best) best = d;
    }
    return best;
}

static int assertMinDistanceToBorder(const std::string &name, const Polygon &waypoints,
                                     const Polygon &perimeter, double expected) {
    double minDist = std::numeric_limits<double>::max();
    Point worst{0, 0};
    for (const auto &p : waypoints) {
        double d = pointToPolygonDistance(p, perimeter);
        // Points that lie exactly on the perimeter boundary are allowed (e.g.
        // border-lap waypoints or driving along a narrow peninsula). Only
        // points that are inside the perimeter but closer than expected count.
        if (d < 1e-3) continue;
        if (d < minDist) { minDist = d; worst = p; }
    }
    if (minDist < expected - 1e-3) {
        std::cerr << "[" << name << "] distance to border too small: min=" << minDist
                  << " expected>=" << expected << " at (" << worst.X << "," << worst.Y << ")" << std::endl;
        return 1;
    }
    std::cout << "[" << name << "] min distance to border " << minDist << " (expected " << expected << ")" << std::endl;
    return 0;
}

static Polygon makePolygon(int n, double radius, double cx, double cy, double irregularity = 0.35, int spurStep = -1, double spurLen = 0.0) {
    Polygon poly;
    poly.reserve(n + 3);
    std::mt19937 gen(42 + n);
    std::uniform_real_distribution<> dist(0.0, 1.0);
    for (int i = 0; i < n; i++) {
        double angle = 2.0 * M_PI * i / n;
        double r = radius * (1.0 - irregularity + 2.0 * irregularity * dist(gen));
        double x = cx + r * std::cos(angle);
        double y = cy + r * std::sin(angle);
        poly.push_back({x, y});

        if (i == spurStep && spurLen > 0.0) {
            double dx = x - cx;
            double dy = y - cy;
            double len = std::hypot(dx, dy);
            double nx = dx / len;
            double ny = dy / len;
            double tx = -ny;
            double ty = nx;
            double sx = x + nx * spurLen * 0.6;
            double sy = y + ny * spurLen * 0.6;
            poly.push_back({sx + tx * spurLen * 0.25, sy + ty * spurLen * 0.25});
            poly.push_back({sx + nx * spurLen, sy + ny * spurLen});
            poly.push_back({sx - tx * spurLen * 0.25, sy - ty * spurLen * 0.25});
            poly.push_back({x, y});
        }
    }
    poly.push_back(poly[0]);
    return poly;
}

static void makeOutDir() {
    static bool created = false;
    if (!created) {
        if (std::system("mkdir -p out") != 0) {
            std::cerr << "Failed to create out directory" << std::endl;
        }
        created = true;
    }
}

static std::string safeFileName(const std::string &name) {
    std::string s = name;
    for (char &c : s) {
        if (c == ' ' || c == '/') c = '_';
    }
    return s;
}

static void polygonBounds(const Polygon &poly, double &minX, double &minY, double &maxX, double &maxY) {
    minX = minY = std::numeric_limits<double>::max();
    maxX = maxY = -std::numeric_limits<double>::max();
    for (const auto &p : poly) {
        if (p.X < minX) minX = p.X;
        if (p.X > maxX) maxX = p.X;
        if (p.Y < minY) minY = p.Y;
        if (p.Y > maxY) maxY = p.Y;
    }
}

static void mapBounds(const Map &map, double &minX, double &minY, double &maxX, double &maxY) {
    polygonBounds(map.perimeter, minX, minY, maxX, maxY);
    for (const auto &excl : map.exclusions) {
        double eminX, eminY, emaxX, emaxY;
        polygonBounds(excl, eminX, eminY, emaxX, emaxY);
        minX = std::min(minX, eminX);
        minY = std::min(minY, eminY);
        maxX = std::max(maxX, emaxX);
        maxY = std::max(maxY, emaxY);
    }
}

static std::string pointsToPath(const Polygon &poly) {
    std::ostringstream oss;
    for (const auto &p : poly) {
        if (!oss.str().empty()) oss << " ";
        oss << p.X << "," << p.Y;
    }
    return oss.str();
}

static void writeSvg(const std::string &name, const Map &map, const Polygon &waypoints) {
    makeOutDir();
    std::string filename = "out/" + safeFileName(name) + ".svg";
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "[" << name << "] failed to open " << filename << std::endl;
        return;
    }

    double minX, minY, maxX, maxY;
    mapBounds(map, minX, minY, maxX, maxY);
    double padding = std::max((maxX - minX), (maxY - minY)) * 0.05 + 0.5;
    minX -= padding; minY -= padding; maxX += padding; maxY += padding;
    double width = maxX - minX;
    double height = maxY - minY;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"800\" "
        << "viewBox=\"" << minX << " " << minY << " " << width << " " << height << "\">\n";
    out << "<rect x=\"" << minX << "\" y=\"" << minY << "\" width=\"" << width
        << "\" height=\"" << height << "\" fill=\"white\"/>\n";

    // Perimeter
    out << "<polygon points=\"" << pointsToPath(map.perimeter) << "\" "
        << "fill=\"none\" stroke=\"black\" stroke-width=\"0.08\"/>\n";

    // Exclusions
    for (const auto &excl : map.exclusions) {
        out << "<polygon points=\"" << pointsToPath(excl) << "\" "
            << "fill=\"rgba(255,0,0,0.2)\" stroke=\"red\" stroke-width=\"0.06\"/>\n";
    }

    // Route
    if (waypoints.size() >= 2) {
        out << "<polyline points=\"" << pointsToPath(waypoints) << "\" "
            << "fill=\"none\" stroke=\"blue\" stroke-width=\"0.04\"/>\n";
    }

    // Start point
    if (!waypoints.empty()) {
        out << "<circle cx=\"" << waypoints[0].X << "\" cy=\"" << waypoints[0].Y
            << "\" r=\"0.15\" fill=\"green\"/>\n";
    }

    out << "</svg>\n";
    std::cout << "[" << name << "] wrote " << filename << std::endl;
}

static int checkScenario(const std::string &name, Map &map, Settings &settings, double expectedMinDistToBorder = -1.0) {
    auto waypoints = calculateWaypoints(map, settings, nullptr);
    writeSvg(name, map, waypoints);
    int violations = 0;
    for (size_t i = 1; i < waypoints.size(); i++) {
        for (const auto &excl : map.exclusions) {
            if (segmentInsideExclusion(waypoints[i - 1], waypoints[i], excl, map.perimeter)) {
                // When the user explicitly wants to mow the exclusion border,
                // it is acceptable for a segment to lie exactly on the
                // shared boundary between the perimeter and the exclusion.
                // It must never enter the interior of the exclusion, which
                // segmentInsideExclusion already checks for.
                if (settings.mowExclusionBorder) {
                    bool onSharedBoundary = true;
                    int samples = std::max(3, (int)(std::hypot(waypoints[i].X - waypoints[i - 1].X,
                                                              waypoints[i].Y - waypoints[i - 1].Y) / 0.05));
                    if (samples > 50) samples = 50;
                    for (int k = 0; k <= samples; k++) {
                        double t = (double)k / samples;
                        Point p{waypoints[i - 1].X + (waypoints[i].X - waypoints[i - 1].X) * t,
                                waypoints[i - 1].Y + (waypoints[i].Y - waypoints[i - 1].Y) * t};
                        if (!pointOnBoundary(p, excl, 1e-6) || !pointOnBoundary(p, map.perimeter, 1e-6)) {
                            onSharedBoundary = false;
                            break;
                        }
                    }
                    if (onSharedBoundary) continue;
                }
                if (violations == 0) std::cerr << "[" << name << "] violations:" << std::endl;
                std::cerr << "  segment " << i - 1 << " (" << waypoints[i - 1].X << "," << waypoints[i - 1].Y << ") -> ("
                          << waypoints[i].X << "," << waypoints[i].Y << ")" << std::endl;
                violations++;
            }
        }
    }
    if (expectedMinDistToBorder >= 0.0) {
        violations += assertMinDistanceToBorder(name, waypoints, map.perimeter, expectedMinDistToBorder);
    }
    std::cout << "[" << name << "] " << waypoints.size() << " waypoints, " << violations << " violations" << std::endl;
    return violations;
}

int main() {
    int total = 0;

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("center 0 deg", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 45;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("center 45 deg", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 2;
        settings.width = 0.16f;
        settings.angle = 0;
        settings.distanceToBorder = 1;
        settings.borderLaps = 1;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("rings user settings", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 2;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("rings center exclusion", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 0}, {7, 0}, {7, 4}, {3, 4}, {3, 0}}}};
        Settings settings;
        settings.pattern = 2;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("rings edge exclusion", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {
            {{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}},
            {{{1, 8}, {2, 8}, {2, 9}, {1, 9}, {1, 8}}}
        };
        Settings settings;
        settings.pattern = 2;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("rings two holes", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 1;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("center crosshatch", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {
            {{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}},
            {{{1, 8}, {2, 8}, {2, 9}, {1, 9}, {1, 8}}}
        };
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("two holes", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 0}, {7, 0}, {7, 4}, {3, 4}, {3, 0}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("hole on edge", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 1;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("center with border offset", map, settings, 0.5);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 1;
        settings.mowArea = false;
        settings.mowBorderCcw = false;
        total += checkScenario("border laps 1", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 2;
        settings.mowArea = false;
        settings.mowBorderCcw = false;
        total += checkScenario("border laps 2", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        map.exclusions = {{{{3, 0}, {7, 0}, {7, 4}, {3, 4}, {3, 0}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 2;
        settings.mowArea = false;
        settings.mowExclusionBorder = true;
        settings.mowBorderCcw = false;
        total += checkScenario("border laps edge exclusion", map, settings);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 2;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("distance to border 2", map, settings, 1.0);
    }

    {
        Map map;
        map.perimeter = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 3;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("distance to border 3", map, settings, 1.5);
    }

    {
        Map map;
        map.perimeter = makePolygon(20, 10.0, 10.0, 10.0);
        map.exclusions = {{{{7, 7}, {13, 7}, {13, 13}, {7, 13}, {7, 7}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("20pt center exclusion", map, settings);
    }

    {
        Map map;
        map.perimeter = makePolygon(20, 10.0, 10.0, 10.0);
        map.exclusions = {{{{7, 7}, {13, 7}, {13, 13}, {7, 13}, {7, 7}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 2;
        settings.mowArea = false;
        settings.mowExclusionBorder = true;
        settings.mowBorderCcw = false;
        total += checkScenario("20pt border laps", map, settings);
    }

    {
        Map map;
        map.perimeter = makePolygon(20, 10.0, 10.0, 10.0);
        map.exclusions = {{{{7, 7}, {13, 7}, {13, 13}, {7, 13}, {7, 7}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 2;
        settings.borderLaps = 0;
        settings.mowArea = true;
        settings.mowBorderCcw = false;
        total += checkScenario("20pt distance to border 2", map, settings, 0.95);
    }

    {
        Map map;
        map.perimeter = makePolygon(20, 10.0, 10.0, 10.0);
        map.exclusions = {{{{10, 0}, {14, 0}, {14, 6}, {10, 6}, {10, 0}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 0;
        settings.borderLaps = 2;
        settings.mowArea = false;
        settings.mowExclusionBorder = true;
        settings.mowBorderCcw = false;
        total += checkScenario("20pt edge exclusion border laps", map, settings);
    }

    {
        Map map;
        map.perimeter = makePolygon(20, 10.0, 10.0, 10.0, 0.35, 5, 6.0);
        map.exclusions = {{{{8, 8}, {12, 8}, {12, 12}, {8, 12}, {8, 8}}}};
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.5f;
        settings.angle = 0;
        settings.distanceToBorder = 1;
        settings.borderLaps = 1;
        settings.mowArea = false;
        settings.mowBorderCcw = false;
        total += checkScenario("20pt spur with border lap", map, settings);
    }

    {
        Map map;
        // CassandRA/ArduMower Modem JSON export (Y-down screen coordinates)
        // Converted to path planner Y-up coordinates by negating y values.
        map.perimeter = {
            {4.82, 10.21}, {4.65, 10.31}, {4.84, 10.32}, {5.18, 10.14}, {5.41, 9.99},
            {5.79, 9.62}, {5.71, 9.38}, {6.22, 8.77}, {6.52, 8.86}, {7.05, 8.46},
            {7.26, 8.28}, {7.41, 8.26}, {7.88, 8.04}, {9.19, 9.429999}, {10.35, 8.349998},
            {10.04, 8.309999}, {9.88, 8.17}, {9.71, 8.01}, {9.58, 7.72}, {9.52, 7.45},
            {9.28, 7.45}, {9.139999, 7.31}, {7.08, 5.34}, {6.99, 5.04}, {7.03, 4.88},
            {7.200765133, 3.921196222}, {5.73, 3.17}, {5.51, 3.45}, {6.28, 4.66},
            {3.03, 7.8}, {3.4, 8.02}, {3.73, 8.179996}, {4, 8.33}, {4.16, 8.53},
            {4.26, 8.7}, {4.26, 8.98}, {4.75, 9.429999}, {4.92, 9.5}, {5.02, 9.69},
            {5.06, 9.88}, {5.03, 10.01}, {4.82, 10.21}
        };
        map.exclusions = {
            {
                {4.719904, 7.044162}, {5.843588352, 7.190095425}, {5.653875351, 6.21234417}, {4.719904, 7.044162}
            }
        };
        map.dockpoints = {};
        map.waypoints = {
            {5.212, 9.926001}, {5.305, 9.865}, {5.605, 9.573}, {5.53, 9.346}, {6.164, 8.585},
            {6.487, 8.682}, {6.953, 8.332}, {7.191, 8.126}, {7.364, 8.104}, {7.916, 7.845},
            {9.196, 9.203}, {9.991, 8.462999}, {9.969999, 8.461}, {9.772, 8.289001}, {9.577, 8.105},
            {9.426001, 7.77}, {9.391, 7.609}, {9.212999, 7.609}, {9.028, 7.424}, {6.938, 5.425},
            {6.823, 5.043}, {6.871, 4.852}, {7.021, 4.008}, {5.774, 3.371}, {5.705, 3.459},
            {6.484, 4.683}, {3.291, 7.768}, {3.474, 7.877}, {3.804, 8.037}, {4.105, 8.205},
            {4.291, 8.438}, {4.419, 8.655}, {4.419, 8.908}, {4.836, 9.291}, {5.035, 9.375},
            {5.171, 9.634}, {5.223, 9.882}, {5.33, 9.615}, {5.421, 9.526999}, {5.35, 9.312},
            {6.109, 8.401}, {6.455, 8.505}, {6.848, 8.211}, {7.123, 7.972}, {7.319, 7.947},
            {7.953, 7.651}, {9.203, 8.977}, {9.742, 8.474999}, {9.665, 8.408}, {9.444, 8.2},
            {9.273, 7.82}, {9.262, 7.769}, {9.147, 7.769}, {8.947, 7.569}, {6.797, 5.511},
            {6.657, 5.047}, {6.713, 4.824}, {6.842, 4.096}, {6.038, 3.685}, {6.689, 4.707},
            {3.552, 7.737}, {3.878, 7.895}, {4.21, 8.081}, {4.423, 8.347}, {4.579, 8.611},
            {4.579, 8.837}, {4.924, 9.155}, {5.15, 9.25}, {5.323, 9.579}, {6.637, 4.328},
            {6.663, 4.184}, {6.488, 4.094}, {6.507, 5.104}, {3.827, 7.692}, {3.952, 7.753},
            {4.315, 7.956}, {4.555, 8.256001}, {4.739, 8.567}, {4.739, 8.766}, {5.012, 9.019},
            {5.265, 9.125}, {5.277, 9.148}, {6.054, 8.217}, {6.423, 8.328}, {6.743, 8.09},
            {7.055, 7.819}, {7.274, 7.791}, {7.99, 7.457}, {9.21, 8.751}, {9.504, 8.477},
            {9.311, 8.295}, {9.147, 7.929}, {9.081, 7.929}, {8.836, 7.684}, {6.656, 5.597},
            {6.515, 5.683}, {6.429, 5.4}, {4.098, 7.651}, {4.42, 7.832}, {4.687, 8.165},
            {4.899, 8.523}, {4.899, 8.696}, {5.098, 8.881001}, {5.24, 8.941}, {5.999, 8.033},
            {6.361, 8.151}, {6.537, 7.843}, {6.919, 7.513}, {7.184, 7.479}, {7.139, 7.322999},
            {7.558, 7.127}, {6.324, 5.944}, {4.628, 7.582}, {4.951, 7.982}, {5.219, 8.434999},
            {5.219, 8.464}, {5.889, 7.664}, {6.329, 7.797}, {6.432, 7.722}, {6.851, 7.36},
            {6.783, 7.207}, {7.094, 7.167}, {7.278, 7.08}, {6.324, 6.165}, {4.844, 7.595},
            {5.083, 7.891}, {5.251, 8.175}, {5.834, 7.48}, {6.299, 7.621}, {6.327, 7.601},
            {6.264, 7.443}, {6.715, 7.054}, {6.984, 7.019}, {6.324, 6.386}, {5.06, 7.608},
            {5.215, 7.8}, {5.273, 7.898}, {5.779, 7.296}, {5.784, 7.13}, {6.225, 7.264},
            {6.638, 6.909}, {6.324, 6.607}, {5.384, 7.516}, {5.276, 7.621}, {5.286, 7.633},
            {6.09, 7.055}, {6.187, 7.085}, {6.4, 6.902}, {6.324, 6.829}, {5.218899554, 5.685186053},
            {3.03, 7.8}, {3.4, 8.02}, {3.73, 8.179996}, {4, 8.33}, {4.16, 8.53}, {4.26, 8.7},
            {4.26, 8.98}, {4.75, 9.429999}, {4.92, 9.5}, {5.02, 9.69}, {5.06, 9.88}, {5.03, 10.01},
            {4.82, 10.21}, {4.65, 10.31}, {4.84, 10.32}, {5.18, 10.14}, {5.41, 9.99}, {5.79, 9.62},
            {5.71, 9.38}, {6.22, 8.77}, {6.52, 8.86}, {7.05, 8.46}, {7.26, 8.28}, {7.41, 8.26},
            {7.88, 8.04}, {9.19, 9.429999}, {10.35, 8.349998}, {10.04, 8.309999}, {9.88, 8.17},
            {9.71, 8.01}, {9.58, 7.72}, {9.52, 7.45}, {9.28, 7.45}, {9.139999, 7.31}, {7.08, 5.34},
            {9.139999, 7.31}, {9.28, 7.45}, {9.52, 7.45}, {9.58, 7.72}, {9.71, 8.01}, {9.88, 8.17},
            {10.04, 8.309999}, {10.35, 8.349998}, {9.19, 9.429999}, {7.88, 8.04}, {7.41, 8.26},
            {7.26, 8.28}, {7.05, 8.46}, {6.52, 8.86}, {6.22, 8.77}, {5.71, 9.38}, {5.79, 9.62},
            {5.41, 9.99}, {5.18, 10.14}, {4.84, 10.32}, {4.65, 10.31}, {4.82, 10.21}, {5.03, 10.01},
            {5.06, 9.88}, {5.02, 9.69}, {4.92, 9.5}, {4.75, 9.429999}, {4.26, 8.98}, {4.26, 8.7},
            {4.16, 8.53}, {4, 8.33}, {3.73, 8.179996}, {3.4, 8.02}, {3.03, 7.8}, {6.28, 4.66},
            {5.51, 3.45}, {5.73, 3.17}, {7.200765133, 3.921196222}, {7.03, 4.88}, {6.99, 5.04},
            {7.08, 5.34}, {5.212, 9.926001}
        };
        Settings settings;
        settings.pattern = 0;
        settings.width = 0.35f;
        settings.angle = 0;
        settings.distanceToBorder = 0.5;
        settings.borderLaps = 1;
        settings.mowArea = true;
        settings.mowExclusionBorder = true;
        settings.mowBorderCcw = false;
        total += checkScenario("cassandra user map 0.35m border offset", map, settings, 0.15);
    }

    if (total == 0) {
        std::cout << "\nPASS: no segment intersects any exclusion" << std::endl;
        return 0;
    }
    std::cerr << "\nFAIL: " << total << " segment(s) intersect exclusions" << std::endl;
    return 1;
}
