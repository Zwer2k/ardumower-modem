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

std::vector<Polygon> offsetPolygonInward(const Polygon &poly, double dist) {
    if (dist <= 0.001 || poly.size() < 3) return {poly};
    auto result = clipOffset(poly, -dist);
    if (result.empty()) return {};
    return result;
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

// ── Rings pattern (BFS queue + Clipper2 InflatePaths) ──
Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, const Point &startNear)
{
    Log(DBG, "%scalculateRingsPattern width=%.3f", _LOG_, width);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    // Rotate first ring so start is near startNear
    Polygon first = areaToMow;
    size_t firstBest = nearestPointIndex(startNear, first);
    Polygon firstRot;
    firstRot.reserve(first.size());
    for (size_t i = 0; i < first.size(); i++)
        firstRot.push_back(first[(firstBest + i) % first.size()]);

    // BFS ring processing (matches web app calcPatternRings)
    std::vector<Polygon> queue;
    queue.push_back(firstRot);

    Polygon route;

    while (!queue.empty()) {
        Polygon currentArea = queue.front();
        queue.erase(queue.begin());
        if (currentArea.size() < 3) continue;

        // Rotate ring so start is near previous end
        if (!route.empty()) {
            size_t best = nearestPointIndex(route.back(), currentArea);
            Polygon rot;
            rot.reserve(currentArea.size());
            for (size_t i = 0; i < currentArea.size(); i++)
                rot.push_back(currentArea[(best + i) % currentArea.size()]);
            currentArea = rot;
        }

        // Add current ring to route
        for (const auto &p : currentArea)
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);

        // Shrink inward using Clipper2 — push ALL resulting branches to BFS queue
        auto nextList = clipOffset(currentArea, -width);
        double curAreaVal = std::abs(polygonArea(currentArea));
        bool anyAdded = false;
        for (const auto &next : nextList) {
            if (next.size() < 3) continue;
            double nextAreaVal = std::abs(polygonArea(next));
            if (nextAreaVal < 0.001 || nextAreaVal >= curAreaVal - 0.001) continue;
            queue.push_back(next);
            anyAdded = true;
        }
        if (!anyAdded) continue;
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
    Point near = startNear;

    for (int lap = 0; lap < laps; lap++) {
        bool cw = isClockwise(currentRing);
        Polygon ring = currentRing;
        if (ccw == cw) std::reverse(ring.begin(), ring.end());

        size_t startIdx = nearestPointIndex(near, ring);
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

        if (!route.empty()) near = route.back();

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

// Check if line segment from→to crosses any perimeter edge (exits the polygon).
// Uses exact line-segment intersection — unlike point-sampling, this cannot miss
// short dips outside the perimeter.
static bool lineExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter) {
    for (size_t i = 0; i < perimeter.size(); i++) {
        size_t j = (i + 1) % perimeter.size();
        double o1 = crossProduct(perimeter[i], perimeter[j], from);
        double o2 = crossProduct(perimeter[i], perimeter[j], to);
        double o3 = crossProduct(from, to, perimeter[i]);
        double o4 = crossProduct(from, to, perimeter[j]);
        if (o1 * o2 < 0 && o3 * o4 <= 0) return true;
    }
    return false;
}

// ── Connect sorted polys into a single waypoint path ──
void connectPolysUsingPathFinding(Polygon &waypoints, const std::vector<Polygon> &polys,
    const Polygon &perimeter) {
    waypoints.clear();

    for (size_t i = 0; i < polys.size(); i++) {
        const auto &poly = polys[i];
        if (poly.empty()) continue;

        if (i > 0) {
            const Point &from = waypoints.back();
            const Point &to = poly[0];
            double d = distance(from, to);
            bool needWalk = d > 0.1 && lineExitsPerimeter(from, to, perimeter);
            if (needWalk) {
                Polygon conn = walkPerimeter(perimeter, from, to);
                for (size_t k = 0; k < conn.size(); k++)
                    if (distance(waypoints.back(), conn[k]) > 0.01)
                        waypoints.push_back(conn[k]);
            }
        }

        for (size_t j = 0; j < poly.size(); j++)
            if (waypoints.empty() || distance(waypoints.back(), poly[j]) > 0.01)
                waypoints.push_back(poly[j]);
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

    // Collect all area polygons (inward offset may split at bottlenecks)
    std::vector<Polygon> areasToMow;
    if (settings.distanceToBorder > 0 && settings.width > 0.001) {
        double offsetDist = settings.distanceToBorder * settings.width;
        areasToMow = offsetPolygonInward(perimeter, offsetDist);
    }
    if (areasToMow.empty()) areasToMow = {perimeter};
    for (auto it = areasToMow.begin(); it != areasToMow.end(); )
        if (it->size() < 3) it = areasToMow.erase(it); else ++it;
    if (areasToMow.empty()) return {};

    if (settings.borderLaps > 0 && settings.mowBorderCcw) {
        Polygon borderLaps = addBorderLaps(perimeter, settings.borderLaps, true, startNear, settings.width);
        route = borderLaps;
        if (!route.empty()) startNear = route.back();
    }

    if (settings.mowArea) {
        std::vector<Polygon> allSegments;

        if (settings.pattern == 2) {
            // Rings pattern: per-area (each area gets its own concentric rings)
            for (const auto &area : areasToMow) {
                Polygon ar = calculateRingsPattern(perimeter, area, settings.width, startNear);
                if (!ar.empty()) allSegments.push_back(ar);
            }
        } else {
            // Lines pattern: one zigzag across combined bounding box of ALL areas,
            // clipped to each area → uniform track width across area boundaries
            int passes = (settings.pattern == 1) ? 2 : 1;
            for (int pass = 0; pass < passes; pass++) {
                double angleDeg = settings.angle + (pass == 1 ? 90.0 : 0.0);

                // Rotate areas by -angle so zigzag runs along Y
                auto rotatedAreas = rotatePolygons(areasToMow, -angleDeg);

                // Combined bounding box of all rotated areas
                double rMinX, rMinY, rMaxX, rMaxY;
                boundingBox(rotatedAreas[0], rMinX, rMinY, rMaxX, rMaxY);
                for (size_t ai = 1; ai < rotatedAreas.size(); ai++) {
                    double ax, ay, bx, by;
                    boundingBox(rotatedAreas[ai], ax, ay, bx, by);
                    if (ax < rMinX) rMinX = ax;
                    if (ay < rMinY) rMinY = ay;
                    if (bx > rMaxX) rMaxX = bx;
                    if (by > rMaxY) rMaxY = by;
                }

                // Zigzag in rotated space
                double margin = settings.width * 2;
                double xRange = std::max(std::abs(rMinX), std::abs(rMaxX)) + margin;
                double lastX = -xRange;
                Polygon zigzag;
                for (double y = rMinY - margin; y <= rMaxY + margin; y += settings.width) {
                    zigzag.push_back({lastX, y});
                    zigzag.push_back({-lastX, y});
                    lastX = -lastX;
                }

                // Clip to each rotated area and rotate back
                for (const auto &ra : rotatedAreas) {
                    auto clipped = clipIntersectOpen({zigzag}, ra);
                    auto rotated = rotatePolygons(clipped, angleDeg);
                    allSegments.insert(allSegments.end(), rotated.begin(), rotated.end());
                }
            }
        }

        // Sort and connect all segments into a single route
        if (!allSegments.empty()) {
            sortSolutionPolygonsByDistance(allSegments, startNear);
            Polygon pattern;
            connectPolysUsingPathFinding(pattern, allSegments, perimeter);
            pattern = pruneOutside(pattern, perimeter);

            if (!route.empty() && !pattern.empty()) {
                Polygon conn = walkPerimeter(perimeter, route.back(), pattern[0]);
                for (size_t k = 0; k < conn.size(); k++)
                    if (distance(route.back(), conn[k]) > 0.01)
                        route.push_back(conn[k]);
            }
            route.insert(route.end(), pattern.begin(), pattern.end());
            if (!route.empty()) startNear = route.back();
        }
    }

    if (settings.borderLaps > 0 && !settings.mowBorderCcw) {
        Polygon borderLaps = addBorderLaps(perimeter, settings.borderLaps, false, startNear, settings.width);
        if (!route.empty() && !borderLaps.empty()) {
            Polygon conn = walkPerimeter(perimeter, route.back(), borderLaps[0]);
            for (size_t k = 0; k < conn.size(); k++)
                if (distance(route.back(), conn[k]) > 0.01)
                    route.push_back(conn[k]);
        }
        route.insert(route.end(), borderLaps.begin(), borderLaps.end());
    }

    route = pruneOutside(route, perimeter);
    Log(INFO, "%scalculateWaypoints done: %d waypoints", _LOG_, route.size());
    return route;
}

        }
    }
}
#endif
