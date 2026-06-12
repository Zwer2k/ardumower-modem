#ifdef ENABLE_MAP
#include "path_planner.h"
#include "clipper_adapter.h"
#include "log.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <queue>

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

std::vector<Polygon> rotatePolygons(const std::vector<Polygon> &polys, double angleDeg) {
    std::vector<Polygon> result;
    result.reserve(polys.size());
    for (const auto &p : polys) result.push_back(rotatePolygon(p, angleDeg));
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

Polygon offsetPolygonInward(const Polygon &poly, double dist) {
    if (dist <= 0.001 || poly.size() < 3) return poly;
    auto result = clipOffset(poly, -dist);
    if (result.empty()) return {};
    return result[0];
}

// Walk along the perimeter from `from` to `to`, following polygon edges exactly.
static Polygon walkPerimeter(const Polygon &peri, const Point &from, const Point &to) {
    Polygon result;
    if (peri.size() < 3) { result.push_back(to); return result; }
    size_t n = peri.size();

    auto nearestOnBoundary = [&](const Point &p) -> std::pair<size_t, Point> {
        size_t bestEdge = 0;
        Point bestProj = peri[0];
        double bestDist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < n; i++) {
            size_t j = (i + 1) % n;
            double dx = peri[j].X - peri[i].X, dy = peri[j].Y - peri[i].Y;
            double len2 = dx*dx + dy*dy;
            if (len2 < 0.001) continue;
            double tt = std::max(0.0, std::min(1.0,
                ((p.X-peri[i].X)*dx + (p.Y-peri[i].Y)*dy) / len2));
            Point pp = {peri[i].X + tt*dx, peri[i].Y + tt*dy};
            double d = distance(p, pp);
            if (d < bestDist) { bestDist = d; bestEdge = i; bestProj = pp; }
        }
        return {bestEdge, bestProj};
    };

    auto [iFrom, projFrom] = nearestOnBoundary(from);
    auto [iTo, projTo] = nearestOnBoundary(to);

    result.push_back(projFrom);

    if (iFrom == iTo) {
        result.push_back(projTo);
        return result;
    }

    Polygon fwd, rev;
    for (size_t k = (iFrom + 1) % n; ; k = (k + 1) % n) {
        fwd.push_back(peri[k]);
        if (k == iTo) break;
        if (fwd.size() > n + 2) break;
    }
    fwd.push_back(projTo);

    for (size_t k = iFrom; ; ) {
        rev.push_back(peri[k]);
        if (k == (iTo + 1) % n) break;
        if (rev.size() > n + 2) break;
        k = (k + n - 1) % n;
    }
    rev.push_back(projTo);

    auto pathLen = [](const Polygon &p) {
        double d = 0;
        for (size_t i = 1; i < p.size(); i++) d += distance(p[i-1], p[i]);
        return d;
    };
    const auto &path = pathLen(fwd) <= pathLen(rev) ? fwd : rev;
    for (const auto &p : path) result.push_back(p);
    return result;
}

static Point projectToBoundary(const Point &p, const Polygon &poly) {
    if (pointInPolygon(p, poly)) return p;
    Point best = p;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X;
        double dy = poly[j].Y - poly[i].Y;
        double len2 = dx*dx + dy*dy;
        if (len2 < 0.001) continue;
        double t = std::max(0.0, std::min(1.0, ((p.X-poly[i].X)*dx + (p.Y-poly[i].Y)*dy) / len2));
        Point proj = {poly[i].X + t*dx, poly[i].Y + t*dy};
        double d = distance(p, proj);
        if (d < bestDist) { bestDist = d; best = proj; }
    }
    return best;
}

static Polygon pruneOutside(const Polygon &waypoints, const Polygon &area) {
    if (area.size() < 3) return waypoints;
    Polygon result;
    result.reserve(waypoints.size());
    for (const auto &wp : waypoints) {
        if (pointInPolygon(wp, area)) {
            if (result.empty() || distance(result.back(), wp) > 0.01)
                result.push_back(wp);
        } else {
            Point snapped = projectToBoundary(wp, area);
            if (result.empty() || distance(result.back(), snapped) > 0.01)
                result.push_back(snapped);
        }
    }
    return result;
}

// ── Lines pattern (zigzag clipped to polygon via Clipper2 Intersection) ──
Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg)
{
    Log(DBG, "%scalculateLinesPattern width=%.3f angle=%.1f", _LOG_, width, angleDeg);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    // Build zigzag path bounded to areaToMow's bounding box + margin
    // (the web app uses -20000 to +20000, but that's too many points for ESP32)
    double minX, minY, maxX, maxY;
    boundingBox(areaToMow, minX, minY, maxX, maxY);
    double margin = width * 2;
    double xRange = std::max(std::abs(minX), std::abs(maxX)) + margin;

    double lastX = -xRange;
    Polygon zigzag;
    int iterCount = 0;
    for (double y = minY - margin; y <= maxY + margin || iterCount % 2 == 1; y += width) {
        zigzag.push_back({lastX, y});
        zigzag.push_back({-lastX, y});
        lastX = -lastX;
        iterCount++;
    }

    // Clip zigzag to areaToMow using Clipper2 Intersection
    // (web app uses ctDifference which for open paths clips to OUTSIDE;
    //  we need INSIDE, so use ctIntersection)
    std::vector<Polygon> polys = clipIntersect(std::vector<Polygon>{zigzag}, areaToMow);
    if (polys.empty()) return {};

    // sortSolutionPolygonsByDistance (matches web app)
    Point startNear = perimeter.empty() ? Point{0,0} : rotatePoint(perimeter[0], -angleDeg);
    sortSolutionPolygonsByDistance(polys, startNear);

    // Connect polys into a single route (matches web app connectPolysUsingPathFinding)
    Polygon route;
    connectPolysUsingPathFinding(route, polys, perimeter);
    route = pruneOutside(route, perimeter);
    Log(DBG, "%scalculateLinesPattern done: %d points", _LOG_, route.size());
    return route;
}

// ── Rings pattern (BFS queue + Clipper2 InflatePaths) ──
Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width)
{
    Log(DBG, "%scalculateRingsPattern width=%.3f", _LOG_, width);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    // BFS ring processing (matches web app calcPatternRings)
    std::vector<Polygon> queue;
    queue.push_back(areaToMow);

    Polygon route;

    while (!queue.empty()) {
        Polygon currentArea = queue.front();
        queue.erase(queue.begin());
        if (currentArea.size() < 3) continue;

        // Add current ring to route
        for (const auto &p : currentArea)
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);

        // Shrink inward using Clipper2
        auto nextList = clipOffset(currentArea, -width);
        if (nextList.empty() || nextList[0].size() < 3) continue;

        Polygon next = nextList[0];
        double nextAreaVal = std::abs(polygonArea(next));
        double curAreaVal = std::abs(polygonArea(currentArea));
        if (nextAreaVal < 0.001 || nextAreaVal >= curAreaVal - 0.001) continue;

        queue.push_back(next);
    }

    route = pruneOutside(route, perimeter);
    Log(DBG, "%scalculateRingsPattern done: %d points", _LOG_, route.size());
    return route;
}

// ── Border laps (offset inward between laps via Clipper2) ──
Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear, double width)
{
    Log(DBG, "%saddBorderLaps laps=%d ccw=%d width=%.3f", _LOG_, laps, ccw, width);
    if (laps <= 0 || perimeter.size() < 3) return {};

    Polygon route;
    Polygon currentRing = perimeter;

    for (int lap = 0; lap < laps; lap++) {
        bool cw = isClockwise(currentRing);
        Polygon ring = currentRing;
        if (ccw == cw) std::reverse(ring.begin(), ring.end());

        size_t startIdx = nearestPointIndex(startNear, ring);
        Polygon ordered;
        for (size_t i = 0; i < ring.size(); i++) {
            size_t idx = (startIdx + i) % ring.size();
            if (ordered.empty() || distance(ordered.back(), ring[idx]) > 0.01)
                ordered.push_back(ring[idx]);
        }
        if (!ordered.empty() && distance(ordered.back(), ordered[0]) > 0.01)
            ordered.push_back(ordered[0]);

        for (const auto &p : ordered)
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);

        if (lap + 1 < laps) {
            auto nextList = clipOffset(currentRing, -width);
            if (nextList.empty() || nextList[0].size() < 3) break;
            currentRing = nextList[0];
        }
    }

    Log(DBG, "%saddBorderLaps done: %d points", _LOG_, route.size());
    return route;
}

// ── Sort solution polygons by nearest-neighbor distance from a start point ──
void sortSolutionPolygonsByDistance(std::vector<Polygon> &solution, const Point &startPt) {
    std::vector<Polygon> sorted;
    sorted.reserve(solution.size());
    Point currentPos = startPt;

    while (!solution.empty()) {
        double minDist = 160000;
        size_t minPolyIdx = 0;
        size_t minPtIdx = 0;
        bool reverse = false;

        for (size_t i = 0; i < solution.size(); i++) {
            size_t n = solution[i].size();
            if (n == 0) continue;

            double dFirst = distance(currentPos, solution[i][0]);
            double dLast = distance(currentPos, solution[i][n - 1]);
            if (dFirst < minDist) {
                minDist = dFirst; minPolyIdx = i; minPtIdx = 0; reverse = false;
            }
            if (dLast < minDist) {
                minDist = dLast; minPolyIdx = i; minPtIdx = n - 1; reverse = true;
            }
        }

        Polygon poly = solution[minPolyIdx];
        solution.erase(solution.begin() + minPolyIdx);

        if (reverse) {
            Polygon rev;
            rev.reserve(poly.size());
            for (int i = (int)poly.size() - 1; i >= 0; i--)
                rev.push_back(poly[i]);
            poly = rev;
        }

        sorted.push_back(poly);
        if (!poly.empty()) currentPos = poly.back();
    }

    solution = sorted;
}

// ── Connect sorted polys into a single waypoint path ──
void connectPolysUsingPathFinding(Polygon &waypoints, const std::vector<Polygon> &polys,
    const Polygon &perimeter) {
    waypoints.clear();
    Point lastPoint;

    for (size_t i = 0; i < polys.size(); i++) {
        const auto &poly = polys[i];
        for (size_t j = 0; j < poly.size(); j++) {
            if (i > 0 && j == 0) {
                // Direct line from end of previous swath to start of next swath
                if (distance(lastPoint, poly[0]) > 0.01) {
                    waypoints.push_back(poly[0]);
                }
            } else {
                if (waypoints.empty() || distance(waypoints.back(), poly[j]) > 0.01)
                    waypoints.push_back(poly[j]);
            }
            lastPoint = poly[j];
        }
    }
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
            route.empty() ? startNear : route.back(), settings.width);
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
#endif
