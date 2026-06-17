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

// Find the nearest edge and the corresponding projection of p onto poly.
static void nearestOnBoundary(const Point &p, const Polygon &poly,
    size_t &edgeIdx, Point &proj)
{
    edgeIdx = 0;
    proj = poly.empty() ? p : poly[0];
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx*dx + dy*dy;
        if (len2 < 0.001) continue;
        double tt = std::max(0.0, std::min(1.0,
            ((p.X - poly[i].X)*dx + (p.Y - poly[i].Y)*dy) / len2));
        Point pp = {poly[i].X + tt*dx, poly[i].Y + tt*dy};
        double d = distance(p, pp);
        if (d < bestDist) { bestDist = d; edgeIdx = i; proj = pp; }
    }
}

// Walk along the perimeter from `from` to `to`, following polygon edges.
// Both endpoints are projected onto their nearest edge. Because the projected
// point can travel to either end of its edge, we evaluate the four possible
// start/end vertex combinations and pick the shortest valid route around the
// polygon.
static Polygon walkPerimeter(const Polygon &peri, const Point &from, const Point &to) {
    Polygon result;
    if (peri.size() < 3) { result.push_back(to); return result; }
    size_t n = peri.size();

    size_t iFrom, iTo;
    Point projFrom, projTo;
    nearestOnBoundary(from, peri, iFrom, projFrom);
    nearestOnBoundary(to, peri, iTo, projTo);

    result.push_back(projFrom);

    if (iFrom == iTo || distance(projFrom, projTo) < 0.001) {
        result.push_back(projTo);
        return result;
    }

    auto pathLen = [](const Polygon &p) {
        double d = 0;
        for (size_t i = 1; i < p.size(); i++) d += distance(p[i-1], p[i]);
        return d;
    };

    // Build a path that starts at projFrom, walks forward through polygon
    // vertices from startPt to endPt (inclusive), and ends at projTo.
    auto buildPath = [&](size_t startPt, size_t endPt) -> Polygon {
        Polygon path;
        path.push_back(projFrom);
        if (startPt != endPt) {
            for (size_t k = startPt; ; k = (k + 1) % n) {
                path.push_back(peri[k]);
                if (k == endPt) break;
                if (path.size() > n + 3) break;
            }
        }
        path.push_back(projTo);
        return path;
    };

    // Four candidates: start at either end of iFrom, end at either end of iTo.
    std::vector<Polygon> candidates;
    candidates.push_back(buildPath((iFrom + 1) % n, iTo));
    candidates.push_back(buildPath((iFrom + 1) % n, (iTo + 1) % n));
    candidates.push_back(buildPath(iFrom, iTo));
    candidates.push_back(buildPath(iFrom, (iTo + 1) % n));

    double bestLen = std::numeric_limits<double>::max();
    for (const auto &c : candidates) {
        double len = pathLen(c);
        if (len < bestLen) { bestLen = len; result = c; }
    }
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

// Returns true if p lies within distance sqrt(tolSq) of any edge of poly.
static bool pointOnBoundary(const Point &p, const Polygon &poly, double tolSq = 1e-6) {
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X;
        double dy = poly[j].Y - poly[i].Y;
        double len2 = dx * dx + dy * dy;
        if (len2 < 1e-10) continue;
        double tt = std::max(0.0, std::min(1.0,
            ((p.X - poly[i].X) * dx + (p.Y - poly[i].Y) * dy) / len2));
        double px = poly[i].X + tt * dx;
        double py = poly[i].Y + tt * dy;
        if ((p.X - px) * (p.X - px) + (p.Y - py) * (p.Y - py) <= tolSq)
            return true;
    }
    return false;
}

// Check if line segment from→to leaves the perimeter polygon.
// The exact edge-crossing test catches most cases, but when both endpoints sit
// on the boundary (common for clipped mowing segments) it can miss a straight
// chord that cuts across the outside of a concave area. We therefore sample
// points along the segment and require every sample to lie inside/on the
// perimeter.
static bool lineExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter) {
    // Standard segment-vs-segment intersection test.
    for (size_t i = 0; i < perimeter.size(); i++) {
        size_t j = (i + 1) % perimeter.size();
        double o1 = crossProduct(perimeter[i], perimeter[j], from);
        double o2 = crossProduct(perimeter[i], perimeter[j], to);
        double o3 = crossProduct(from, to, perimeter[i]);
        double o4 = crossProduct(from, to, perimeter[j]);
        if (o1 * o2 < 0 && o3 * o4 <= 0) return true;
    }

    // Boundary-endpoint fallback: sample the interior of the segment.
    // A point exactly on the boundary is treated as inside to avoid false
    // positives for segments that run along an edge.
    double segLen = distance(from, to);
    if (segLen > 1e-6) {
        double step = 0.10; // 10 cm samples
        int samples = std::max(1, (int)(segLen / step));
        if (samples > 20) samples = 20; // cap computational cost
        for (int k = 1; k < samples; k++) {
            double t = (double)k / samples;
            Point p{from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t};
            // Use a tiny outward margin so boundary samples count as inside.
            if (!pointInPolygon(p, perimeter) && !pointOnBoundary(p, perimeter))
                return true;
        }
    }
    return false;
}

// Sample points along from→to and require each sample to be inside or on the
// boundary of at least one of the given areas.
static bool segmentStaysInAreas(const Point &from, const Point &to,
    const std::vector<Polygon> &areas)
{
    if (areas.empty()) return true;
    double segLen = distance(from, to);
    if (segLen < 1e-6) return true;
    int samples = std::max(5, (int)(segLen / 0.02));
    if (samples > 100) samples = 100;
    for (int k = 0; k <= samples; k++) {
        double t = (double)k / samples;
        Point p{from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t};
        bool insideAny = false;
        for (const auto &area : areas) {
            // Use a 1 mm boundary margin to avoid false positives from numerical
            // noise while still detecting chords that clearly leave the area.
            if (pointInPolygon(p, area) || pointOnBoundary(p, area, 1e-6)) {
                insideAny = true;
                break;
            }
        }
        if (!insideAny) return false;
    }
    return true;
}

// ── Connect sorted polys into a single waypoint path ──
void connectPolysUsingPathFinding(Polygon &waypoints, const std::vector<Polygon> &polys,
    const Polygon &perimeter, const std::vector<Polygon> &areasToMow) {
    waypoints.clear();

    // When the mowable area is an inward offset of the perimeter, use that
    // inner boundary for walk/detection so connectors do not cut through the
    // untraversable border zone. Fall back to the outer perimeter when the
    // offset produced multiple disconnected pieces.
    const Polygon *boundary = &perimeter;
    if (areasToMow.size() == 1 && !areasToMow[0].empty())
        boundary = &areasToMow[0];

    for (size_t i = 0; i < polys.size(); i++) {
        const auto &poly = polys[i];
        if (poly.empty()) continue;

        if (i > 0) {
            const Point &from = waypoints.back();
            const Point &to = poly[0];
            double d = distance(from, to);
            bool exitsBoundary = d > 0.001 && lineExitsPerimeter(from, to, *boundary);
            bool leavesMowArea = d > 0.001 && !segmentStaysInAreas(from, to, areasToMow);
            if (exitsBoundary || leavesMowArea) {
                // If the mowable area is split, walk the outer perimeter so the
                // connector can travel between disconnected areas.
                const Polygon &walkBoundary = areasToMow.size() > 1 ? perimeter : *boundary;
                Polygon conn = walkPerimeter(walkBoundary, from, to);
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
    ArduMower::Domain::Robot::MowSettings &settings,
    const ArduMower::Domain::Robot::State::State *state)
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

    // Prefer start positions close to the rover:
    // 1. If docking, use the end of the dock line (homing line).
    // 2. Otherwise use the current GPS position if it is valid.
    if (state != nullptr) {
        const int JOB_DOCK = 4;
        if (state->job == JOB_DOCK && !map.dockpoints.empty()) {
            const auto &dp = map.dockpoints.back();
            startNear = Point{dp.X, dp.Y};
            Log(DBG, "%sstartNear set to dock line end (%.3f, %.3f)", _LOG_, startNear.X, startNear.Y);
        } else if (state->position.solution > 0 &&
                   (state->position.x != 0.0f || state->position.y != 0.0f)) {
            startNear = Point{state->position.x, state->position.y};
            Log(DBG, "%sstartNear set to GPS position (%.3f, %.3f)", _LOG_, startNear.X, startNear.Y);
        }
    }

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
                std::vector<Polygon> passSegments;
                for (const auto &ra : rotatedAreas) {
                    auto clipped = clipIntersectOpen({zigzag}, ra);
                    auto rotated = rotatePolygons(clipped, angleDeg);
                    passSegments.insert(passSegments.end(), rotated.begin(), rotated.end());
                }

                // Sort and connect this pass on its own. This keeps the mower in
                // one pass direction as long as possible and avoids long diagonal
                // connectors that have to cross concave / excluded areas.
                if (!passSegments.empty()) {
                    sortSolutionPolygonsByDistance(passSegments, startNear);
                    Polygon passRoute;
                    connectPolysUsingPathFinding(passRoute, passSegments, perimeter, areasToMow);
                    passRoute = pruneOutside(passRoute, perimeter);
                    if (!passRoute.empty()) allSegments.push_back(passRoute);
                }
            }
        }

        // Sort and connect the per-area/per-pass routes into one final route.
        if (!allSegments.empty()) {
            sortSolutionPolygonsByDistance(allSegments, startNear);
            Polygon pattern;
            connectPolysUsingPathFinding(pattern, allSegments, perimeter, areasToMow);
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
