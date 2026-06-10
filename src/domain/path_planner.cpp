#include "path_planner.h"
#include "log.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdlib>

#define _LOG_ "PathPlanner::"

namespace ArduMower {
    namespace Modem {
        namespace PathPlanner {

static const double _PI = 3.14159265358979323846;
static const double DEG2RAD = _PI / 180.0;

double distance(const Point &a, const Point &b) {
    double dx = a.X - b.X;
    double dy = a.Y - b.Y;
    return std::sqrt(dx * dx + dy * dy);
}

Point lerp(const Point &a, const Point &b, double t) {
    return Point{a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
}

double crossProduct(const Point &a, const Point &b, const Point &c) {
    return (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X);
}

bool pointInPolygon(const Point &p, const Polygon &poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].Y > p.Y) != (poly[j].Y > p.Y) &&
            p.X < (poly[j].X - poly[i].X) * (p.Y - poly[i].Y) / (poly[j].Y - poly[i].Y) + poly[i].X)
            inside = !inside;
    }
    return inside;
}

Point rotatePoint(const Point &p, double angleDeg) {
    double rad = angleDeg * DEG2RAD;
    double c = std::cos(rad);
    double s = std::sin(rad);
    return Point{p.X * c - p.Y * s, p.X * s + p.Y * c};
}

Polygon rotatePolygon(const Polygon &poly, double angleDeg) {
    Polygon result;
    result.reserve(poly.size());
    for (const auto &p : poly) result.push_back(rotatePoint(p, angleDeg));
    return result;
}

void boundingBox(const Polygon &poly, double &minX, double &minY, double &maxX, double &maxY) {
    minX = minY = std::numeric_limits<double>::max();
    maxX = maxY = -std::numeric_limits<double>::max();
    for (const auto &p : poly) {
        if (p.X < minX) minX = p.X;
        if (p.X > maxX) maxX = p.X;
        if (p.Y < minY) minY = p.Y;
        if (p.Y > maxY) maxY = p.Y;
    }
}

double polygonArea(const Polygon &poly) {
    if (poly.size() < 3) return 0.0;
    double area = 0.0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        area += poly[i].X * poly[j].Y;
        area -= poly[j].X * poly[i].Y;
    }
    return area / 2.0;
}

bool isClockwise(const Polygon &poly) { return polygonArea(poly) < 0; }

size_t nearestPointIndex(const Point &p, const Polygon &poly) {
    if (poly.empty()) return 0;
    size_t best = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        double d = distance(p, poly[i]);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

std::vector<Intersection> intersectRayWithPolygon(double y, const Polygon &poly) {
    std::vector<Intersection> result;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double y1 = poly[i].Y;
        double y2 = poly[j].Y;
        if (std::abs(y2 - y1) < 1e-10) continue;
        if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
            double t = (y - y1) / (y2 - y1);
            double x = poly[i].X + t * (poly[j].X - poly[i].X);
            result.push_back({x, (int)i});
        }
    }
    std::sort(result.begin(), result.end(),
        [](const Intersection &a, const Intersection &b) { return a.x < b.x; });
    return result;
}

Polygon offsetPolygonInward(const Polygon &poly, double distance) {
    if (distance <= 0.001 || poly.size() < 3) return poly;
    Polygon result;
    result.reserve(poly.size());
    bool cw = isClockwise(poly);
    double sign = cw ? -1.0 : 1.0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t prev = (i == 0) ? poly.size() - 1 : i - 1;
        size_t next = (i + 1) % poly.size();
        double dx1 = poly[i].X - poly[prev].X;
        double dy1 = poly[i].Y - poly[prev].Y;
        double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
        double dx2 = poly[next].X - poly[i].X;
        double dy2 = poly[next].Y - poly[i].Y;
        double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
        if (len1 < 0.001 || len2 < 0.001) continue;
        double nx1 = -dy1 / len1 * sign;
        double ny1 = dx1 / len1 * sign;
        double nx2 = -dy2 / len2 * sign;
        double ny2 = dx2 / len2 * sign;
        double bisectorX = nx1 + nx2;
        double bisectorY = ny1 + ny2;
        double bisectorLen = std::sqrt(bisectorX * bisectorX + bisectorY * bisectorY);
        if (bisectorLen < 0.001) continue;
        bisectorX /= bisectorLen;
        bisectorY /= bisectorLen;
        double dot = nx1 * bisectorX + ny1 * bisectorY;
        if (std::abs(dot) < 0.001) continue;
        double offset = distance / dot;
        result.push_back({poly[i].X + bisectorX * offset, poly[i].Y + bisectorY * offset});
    }
    if (result.size() < 3) return poly;
    if (isClockwise(result) != cw) return poly;
    return result;
}

// Check if the straight line between p1 and p2 stays entirely inside the polygon
static bool lineWithinPolygon(const Point &p1, const Point &p2, const Polygon &poly) {
    if (poly.size() < 3) return true;
    double d = distance(p1, p2);
    int steps = std::max(3, (int)(d / 0.05));
    for (int i = 1; i < steps; i++) {
        Point p = lerp(p1, p2, (double)i / steps);
        if (!pointInPolygon(p, poly)) return false;
    }
    return true;
}

// Walk along the perimeter from the nearest point to `from` to the nearest point to `to`.
// Returns perimeter points that act as a valid inside connection.
static Polygon walkPerimeter(const Polygon &peri, const Point &from, const Point &to) {
    Polygon result;
    if (peri.size() < 3) { result.push_back(to); return result; }
    size_t iFrom = nearestPointIndex(from, peri);
    size_t iTo = nearestPointIndex(to, peri);
    if (iFrom == iTo) { result.push_back(to); return result; }
    size_t n = peri.size();
    Polygon pathFwd, pathRev;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (iFrom + i) % n;
        pathFwd.push_back(peri[idx]);
        if (idx == iTo) break;
    }
    for (size_t i = 0; i < n; i++) {
        size_t idx = (iFrom + n - i) % n;
        pathRev.push_back(peri[idx]);
        if (idx == iTo) break;
    }
    double lenFwd = 0, lenRev = 0;
    for (size_t i = 1; i < pathFwd.size(); i++) lenFwd += distance(pathFwd[i-1], pathFwd[i]);
    for (size_t i = 1; i < pathRev.size(); i++) lenRev += distance(pathRev[i-1], pathRev[i]);
    const auto &path = (lenFwd <= lenRev) ? pathFwd : pathRev;
    for (size_t i = 1; i < path.size(); i++)
        result.push_back(path[i]);
    return result;
}

static Polygon pruneOutside(const Polygon &waypoints, const Polygon &area) {
    if (area.size() < 3) return waypoints;
    Polygon result;
    result.reserve(waypoints.size());
    for (const auto &wp : waypoints) {
        if (pointInPolygon(wp, area)) {
            if (result.empty() || distance(result.back(), wp) > 0.01)
                result.push_back(wp);
        }
    }
    return result;
}

struct Swath {
    Point a, b;
    int level;
    bool visited;
};

Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg)
{
    Log(DBG, "%scalculateLinesPattern width=%.3f angle=%.1f", _LOG_, width, angleDeg);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    Polygon areaRotated = rotatePolygon(areaToMow, -angleDeg);
    Polygon periRotated = rotatePolygon(perimeter, -angleDeg);
    double minX, minY, maxX, maxY;
    boundingBox(areaRotated, minX, minY, maxX, maxY);
    if (maxY - minY < 0.01) return {};

    // Create swath list (each swath = one horizontal line segment inside the rotated area)
    std::vector<Swath> swaths;
    double y = minY;
    while (y <= maxY + 0.001) {
        auto crossings = intersectRayWithPolygon(y, areaRotated);
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            double x1 = crossings[i].x;
            double x2 = crossings[i + 1].x;
            if (std::abs(x2 - x1) < 0.02) continue;
            swaths.push_back({{x1, y}, {x2, y}, 0, false});
        }
        y += width;
    }

    Log(DBG, "%scalculateLinesPattern %d swaths", _LOG_, swaths.size());
    if (swaths.empty()) return {};

    // Sort by Y and assign levels (Cassandra: level = progress in Y direction)
    std::sort(swaths.begin(), swaths.end(),
        [](const Swath &a, const Swath &b) { return a.a.Y < b.a.Y; });
    int level = 0;
    double lastY = swaths[0].a.Y;
    for (auto &s : swaths) {
        if (s.a.Y > lastY + 0.001) level++;
        s.level = level;
        lastY = s.a.Y;
    }

    // Rotated start position
    Point start = {0, 0};
    if (!perimeter.empty())
        start = rotatePoint(perimeter[0], -angleDeg);

    // Find nearest swath to start
    {
        size_t nearest = 0;
        double best = std::numeric_limits<double>::max();
        for (size_t i = 0; i < swaths.size(); i++) {
            double d1 = distance(start, swaths[i].a);
            double d2 = distance(start, swaths[i].b);
            if (d1 < best) { best = d1; nearest = i; }
            if (d2 < best) { best = d2; nearest = i; }
        }
        if (nearest != 0) std::swap(swaths[0], swaths[nearest]);
    }

    // Greedy Cassandra-style route building
    Polygon route;
    size_t currentLevel = 0;
    Point currentPos = start;
    int visitedCount = 0;

    // Helper: add a swath (with orientation) to the route
    auto addSwath = [&](const Swath &s, bool reverse) {
        if (reverse) {
            if (route.empty() || distance(route.back(), s.b) > 0.01)
                route.push_back(s.b);
            if (distance(route.back(), s.a) > 0.01)
                route.push_back(s.a);
        } else {
            if (route.empty() || distance(route.back(), s.a) > 0.01)
                route.push_back(s.a);
            if (distance(route.back(), s.b) > 0.01)
                route.push_back(s.b);
        }
    };

    // Activate first swath
    {
        double dA = distance(start, swaths[0].a);
        double dB = distance(start, swaths[0].b);
        addSwath(swaths[0], dB < dA);
        currentPos = route.back();
        currentLevel = swaths[0].level;
        swaths[0].visited = true;
        visitedCount++;
    }

    while (visitedCount < (int)swaths.size()) {
        // Collect candidate swaths (priority to current/adjacent levels, like Cassandra)
        std::vector<size_t> candidates;
        for (size_t i = 0; i < swaths.size(); i++) {
            if (swaths[i].visited) continue;
            int dl = std::abs(swaths[i].level - (int)currentLevel);
            if (dl <= 1) candidates.push_back(i);
        }
        if (candidates.empty()) {
            for (size_t i = 0; i < swaths.size(); i++)
                if (!swaths[i].visited) candidates.push_back(i);
        }

        size_t bestIdx = swaths.size();
        int bestOrient = 0;   // 0 = forward (a->b), 1 = reverse (b->a)
        double bestDist = std::numeric_limits<double>::max();
        Polygon bestPath;

        auto tryConnection = [&](const Point &from, const Point &swathPoint,
                                 const Swath &sw, int orient) {
            // Cassandra step 1: try direct line
            if (lineWithinPolygon(from, swathPoint, areaRotated)) {
                double d = distance(from, swathPoint);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = &sw - swaths.data();
                    bestOrient = orient;
                    bestPath.clear();
                }
                return true;
            }
            // Cassandra step 2: fallback → walk perimeter along boundary
            Polygon conn = walkPerimeter(areaRotated, from, swathPoint);
            // Validate that walk path (from → nearestPeri → ... → swathPoint) stays inside
            bool valid = true;
            Point prev = from;
            for (const auto &p : conn) {
                if (!pointInPolygon(p, areaRotated)) { valid = false; break; }
                if (!lineWithinPolygon(prev, p, areaRotated)) { valid = false; break; }
                prev = p;
            }
            if (valid) {
                double d = distance(from, swathPoint);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = &sw - swaths.data();
                    bestOrient = orient;
                    bestPath = conn;
                }
                return true;
            }
            return false;
        };

        for (size_t ci : candidates) {
            const auto &sw = swaths[ci];
            tryConnection(currentPos, sw.a, sw, 0);
            tryConnection(currentPos, sw.b, sw, 1);
        }

        if (bestIdx < swaths.size()) {
            // Add connection path (if any) + swath
            for (const auto &p : bestPath)
                if (distance(route.back(), p) > 0.01)
                    route.push_back(p);
            addSwath(swaths[bestIdx], bestOrient == 1);
            currentPos = route.back();
            currentLevel = swaths[bestIdx].level;
            swaths[bestIdx].visited = true;
            visitedCount++;
            Log(DBG, "%scalculateLinesPattern visited %d/%d level=%d", _LOG_,
                visitedCount, (int)swaths.size(), currentLevel);
        } else {
            // No valid connection found for any candidate
            // Cassandra: use A* as last resort. Here we add remaining swaths directly
            // and let the mower handle outside segments.
            Log(WARN, "%scalculateLinesPattern no valid connection, adding remaining directly", _LOG_);
            for (auto &sw : swaths) {
                if (sw.visited) continue;
                addSwath(sw, false);
                sw.visited = true;
                visitedCount++;
            }
            break;
        }
    }

    route = rotatePolygon(route, angleDeg);
    route = pruneOutside(route, areaToMow);
    Log(DBG, "%scalculateLinesPattern done: %d points", _LOG_, route.size());
    return route;
}

Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width)
{
    Log(DBG, "%scalculateRingsPattern width=%.3f", _LOG_, width);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    // Cassandra: shrink areaToMow inward by `-width` repeatedly
    // to create concentric rings. Each ring is the full polygon outline,
    // NOT centroid-scaled.
    Polygon route;
    Polygon currentArea = areaToMow;
    Point startNear = perimeter.empty() ? areaToMow[0] : perimeter[0];

    while (true) {
        if (currentArea.size() < 3) {
            // Add centroid as final point
            double minX, minY, maxX, maxY;
            boundingBox(areaToMow, minX, minY, maxX, maxY);
            Point c = {(minX + maxX) / 2.0, (minY + maxY) / 2.0};
            if (route.empty() || distance(route.back(), c) > 0.01)
                route.push_back(c);
            break;
        }

        // Walk the current ring, starting at the point nearest to startNear
        size_t startIdx = nearestPointIndex(startNear, currentArea);
        bool cw = isClockwise(currentArea);
        Polygon ring;
        for (size_t i = 0; i < currentArea.size(); i++) {
            size_t idx = cw ? (startIdx + i) % currentArea.size()
                            : (startIdx + currentArea.size() - i) % currentArea.size();
            ring.push_back(currentArea[idx]);
        }
        for (const auto &p : ring)
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);

        // Shrink inward using buffer-like simplification
        Polygon next = offsetPolygonInward(currentArea, width);
        if (next.size() == currentArea.size() &&
            distance(next[0], currentArea[0]) < 0.001) {
            // offsetPolygonInward didn't actually change the polygon
            break;
        }
        currentArea = next;
        if (isClockwise(currentArea) != cw) break;
    }

    route = pruneOutside(route, areaToMow);
    Log(DBG, "%scalculateRingsPattern done: %d points", _LOG_, route.size());
    return route;
}

Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear)
{
    Log(DBG, "%saddBorderLaps laps=%d ccw=%d", _LOG_, laps, ccw);
    if (laps <= 0 || perimeter.size() < 3) return {};

    Polygon border = perimeter;
    bool cw = isClockwise(border);
    if (ccw == cw) std::reverse(border.begin(), border.end());

    Polygon route;
    for (int lap = 0; lap < laps; lap++) {
        Polygon ring;
        double offsetDist = lap * 0.3;
        if (offsetDist < 0.01) {
            ring = border;
        } else {
            ring.clear();
            for (const auto &p : border) {
                double dx = p.X - border[0].X;
                double dy = p.Y - border[0].Y;
                double len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.01) {
                    ring.push_back({p.X + dx / len * offsetDist, p.Y + dy / len * offsetDist});
                } else {
                    ring.push_back(p);
                }
            }
        }
        if (ring.size() < 3) break;
        size_t startIdx = nearestPointIndex(startNear, ring);
        for (size_t i = 0; i < ring.size(); i++) {
            size_t idx = (startIdx + i) % ring.size();
            route.push_back(ring[idx]);
        }
        route.push_back(ring[startIdx]);
    }

    Log(DBG, "%saddBorderLaps done: %d points", _LOG_, route.size());
    return route;
}

Polygon calculateWaypoints(ArduMower::Domain::Robot::MowerMap &map,
    ArduMower::Domain::Robot::MowSettings &settings)
{
    Log(INFO, "%scalculateWaypoints pattern=%d width=%.3f angle=%d distToBorder=%d borderLaps=%d",
        _LOG_, settings.pattern, settings.width, settings.angle,
        settings.distanceToBorder, settings.borderLaps);

    Polygon perimeter;
    for (const auto &p : map.perimeter) perimeter.push_back(p);
    if (perimeter.size() < 3) {
        Log(WARN, "%sPerimeter too small", _LOG_);
        return {};
    }

    Polygon route;
    Point startNear = perimeter[0];

    Polygon areaToMow;
    if (settings.distanceToBorder > 0 && settings.width > 0.001) {
        double offsetDist = settings.distanceToBorder * settings.width;
        Polygon offset = offsetPolygonInward(perimeter, offsetDist);
        if (offset.size() >= 3) areaToMow = offset;
    }
    if (areaToMow.size() < 3) areaToMow = perimeter;

    if (settings.mowArea && areaToMow.size() >= 3) {
        Polygon pattern;
        if (settings.pattern == 2) {
            pattern = calculateRingsPattern(perimeter, areaToMow, settings.width);
        } else if (settings.pattern == 1) {
            Polygon p1 = calculateLinesPattern(perimeter, areaToMow, settings.width, settings.angle);
            Polygon p2 = calculateLinesPattern(perimeter, areaToMow, settings.width, settings.angle + 90.0);
            pattern.reserve(p1.size() + p2.size());
            pattern.insert(pattern.end(), p1.begin(), p1.end());
            if (!p1.empty() && !p2.empty() && !pointInPolygon(p1.back(), areaToMow))
                pattern.push_back(pattern.back());
            pattern.insert(pattern.end(), p2.begin(), p2.end());
        } else {
            pattern = calculateLinesPattern(perimeter, areaToMow, settings.width, settings.angle);
        }
        if (!route.empty() && !pattern.empty())
            route.push_back(route.back());
        route.insert(route.end(), pattern.begin(), pattern.end());
    }

    if (settings.borderLaps > 0) {
        Polygon borderLaps = addBorderLaps(perimeter, settings.borderLaps, settings.mowBorderCcw,
            route.empty() ? startNear : route.back());
        if (settings.mowBorderCcw) {
            borderLaps.insert(borderLaps.end(), route.begin(), route.end());
            route = borderLaps;
        } else {
            route.insert(route.end(), borderLaps.begin(), borderLaps.end());
        }
    }

    Log(INFO, "%scalculateWaypoints done: %d waypoints", _LOG_, route.size());
    return route;
}

        }
    }
}
