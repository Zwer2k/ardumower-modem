#include "test_stub.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <iostream>

namespace ArduMower {
    namespace Modem {
        namespace PathPlanner {

using Point = ArduMower::Domain::Robot::MapPoint;
using Polygon = std::vector<Point>;
using MowerMap = ArduMower::Domain::Robot::MowerMap;
using MowSettings = ArduMower::Domain::Robot::MowSettings;

#define _LOG_ "PathPlanner::"

struct Intersection {
    double x;
    int edgeIndex;
};

std::vector<Intersection> intersectRayWithPolygon(double y, const Polygon &poly);
Polygon offsetPolygonInward(const Polygon &poly, double distance);
Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg);
Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width);
Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear, double width);
Polygon calculateWaypoints(MowerMap &map, MowSettings &settings);

        }
    }
}

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

// Helper: line intersection of two infinite lines (p1+dir1*t, p2+dir2*s)
// Returns true if intersection exists (not parallel)
static bool lineIntersection(const Point &p1, const Point &dir1,
    const Point &p2, const Point &dir2, double &t)
{
    double det = dir1.X * dir2.Y - dir1.Y * dir2.X;
    if (std::abs(det) < 1e-12) return false;
    double dx = p2.X - p1.X;
    double dy = p2.Y - p1.Y;
    t = (dx * dir2.Y - dy * dir2.X) / det;
    return true;
}

// Remove self-intersections from a polygon using a simple ear-clipping-like approach.
// For each edge, check if it intersects any non-adjacent edge. If so, trim the loop.
static Polygon removeSelfIntersections(const Polygon &poly) {
    if (poly.size() < 4) return poly;
    Polygon res = poly;
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 10) {
        changed = false;
        iterations++;
        size_t n = res.size();
        for (size_t i = 0; i < n && !changed; i++) {
            size_t j = (i + 1) % n;
            for (size_t k = i + 2; k < n && !changed; k++) {
                size_t l = (k + 1) % n;
                if (j == k || l == i) continue;
                Point a1 = res[i], a2 = res[j];
                Point b1 = res[k], b2 = res[l];
                double dx1 = a2.X - a1.X, dy1 = a2.Y - a1.Y;
                double dx2 = b2.X - b1.X, dy2 = b2.Y - b1.Y;
                double det = dx1 * dy2 - dy1 * dx2;
                if (std::abs(det) < 1e-10) continue;
                double t = ((b1.X - a1.X) * dy2 - (b1.Y - a1.Y) * dx2) / det;
                double u = ((b1.X - a1.X) * dy1 - (b1.Y - a1.Y) * dx1) / det;
                if (t > 1e-8 && t < 1 - 1e-8 && u > 1e-8 && u < 1 - 1e-8) {
                    // Intersection found: remove the loop vertices between j and k
                    Point ix{a1.X + t * dx1, a1.Y + t * dy1};
                    Polygon trimmed;
                    trimmed.reserve(n);
                    for (size_t idx = 0; idx <= i; idx++) trimmed.push_back(res[idx]);
                    trimmed.push_back(ix);
                    for (size_t idx = l; idx < n; idx++) trimmed.push_back(res[idx]);
                    res = trimmed;
                    changed = true;
                }
            }
        }
    }
    return res;
}

Polygon offsetPolygonInward(const Polygon &poly, double distance) {
    if (distance <= 0.001 || poly.size() < 3) return poly;
    bool cw = isClockwise(poly);
    double sign = cw ? -1.0 : 1.0;

    // Build offset edges (parallel lines shifted inward)
    struct OffsetEdge {
        Point p1, p2; // original edge endpoints
        Point n;      // inward normal (unit)
    };
    std::vector<OffsetEdge> edges;
    edges.reserve(poly.size());
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X;
        double dy = poly[j].Y - poly[i].Y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001) continue;
        double nx = -dy / len * sign;
        double ny = dx / len * sign;
        edges.push_back({poly[i], poly[j], {nx, ny}});
    }
    if (edges.size() < 3) return poly;

    // Intersect adjacent offset edges to get new vertices
    Polygon result;
    result.reserve(edges.size());
    double miterLimit = distance * 3.0; // prevent extreme spikes

    for (size_t i = 0; i < edges.size(); i++) {
        size_t prev = (i == 0) ? edges.size() - 1 : i - 1;
        const auto &e1 = edges[prev];
        const auto &e2 = edges[i];

        // Offset lines: p + normal*distance
        Point o1a{e1.p1.X + e1.n.X * distance, e1.p1.Y + e1.n.Y * distance};
        Point o1b{e1.p2.X + e1.n.X * distance, e1.p2.Y + e1.n.Y * distance};
        Point o2a{e2.p1.X + e2.n.X * distance, e2.p1.Y + e2.n.Y * distance};
        Point o2b{e2.p2.X + e2.n.X * distance, e2.p2.Y + e2.n.Y * distance};

        Point dir1{o1b.X - o1a.X, o1b.Y - o1a.Y};
        Point dir2{o2b.X - o2a.X, o2b.Y - o2a.Y};

        double t;
        Point vertex;
        if (lineIntersection(o1a, dir1, o2a, dir2, t)) {
            vertex = {o1a.X + dir1.X * t, o1a.Y + dir1.Y * t};
            // Check how far the vertex moved from original corner
            double originalX = e2.p1.X; // shared vertex of e1 and e2
            double originalY = e2.p1.Y;
            double shift = std::sqrt((vertex.X - originalX) * (vertex.X - originalX)
                                   + (vertex.Y - originalY) * (vertex.Y - originalY));
            if (shift > miterLimit) {
                // Clip to miter limit: project back toward original vertex
                double scale = miterLimit / shift;
                vertex.X = originalX + (vertex.X - originalX) * scale;
                vertex.Y = originalY + (vertex.Y - originalY) * scale;
            }
        } else {
            // Parallel edges: use midpoint of the two offset points
            vertex = {(o1b.X + o2a.X) * 0.5, (o1b.Y + o2a.Y) * 0.5};
        }
        result.push_back(vertex);
    }

    if (result.size() < 3) return poly;

    // Remove self-intersections introduced by sharp concave corners
    result = removeSelfIntersections(result);

    if (result.size() < 3) return poly;
    if (isClockwise(result) != cw) return poly;

    // Validate: offset polygon should be mostly inside the original.
    // If the offset was larger than the polygon can support, points will
    // end up outside (effectively an outward offset). In that case return empty.
    int insideCount = 0;
    for (const auto &p : result) {
        if (pointInPolygon(p, poly)) insideCount++;
    }
    if (insideCount < (int)result.size() / 2 + 1) {
        return Polygon();
    }

    return result;
}

// Find the nearest edge and projected point for a given point
static void nearestOnBoundary(const Point &p, const Polygon &poly,
    size_t &edgeIdx, double &t, Point &proj)
{
    edgeIdx = 0; t = 0; proj = poly.empty() ? p : poly[0];
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx*dx + dy*dy;
        if (len2 < 0.001) continue;
        double tt = std::max(0.0, std::min(1.0,
            ((p.X-poly[i].X)*dx + (p.Y-poly[i].Y)*dy) / len2));
        Point pp = {poly[i].X + tt*dx, poly[i].Y + tt*dy};
        double d = distance(p, pp);
        if (d < bestDist) { bestDist = d; edgeIdx = i; t = tt; proj = pp; }
    }
}

// Walk along the perimeter from `from` to `to`, following polygon edges exactly.
// Both from/to are projected onto the nearest edge so points on the boundary
// are handled correctly (not just snapped to vertices).
static Polygon walkPerimeter(const Polygon &peri, const Point &from, const Point &to) {
    Polygon result;
    if (peri.size() < 3) { result.push_back(to); return result; }
    size_t n = peri.size();
    size_t iFrom, iTo;
    double tFrom, tTo;
    Point projFrom, projTo;
    nearestOnBoundary(from, peri, iFrom, tFrom, projFrom);
    nearestOnBoundary(to, peri, iTo, tTo, projTo);

    // Same edge: direct segment along the edge
    if (iFrom == iTo) {
        result.push_back(projFrom);
        result.push_back(projTo);
        return result;
    }

    // Forward: projFrom → peri[iFrom+1] → ... → peri[iTo] → projTo
    Polygon fwd, rev;
    fwd.push_back(projFrom);
    for (size_t k = (iFrom + 1) % n; ; k = (k + 1) % n) {
        fwd.push_back(peri[k]);
        if (k == iTo) break;
        if (fwd.size() > n + 2) break;
    }
    fwd.push_back(projTo);

    // Reverse: projFrom → peri[iFrom] → ... → peri[iTo+1] → projTo
    rev.push_back(projFrom);
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

// Project a point onto the nearest polygon edge
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
            // Snap outside points to nearest boundary edge
            Point snapped = projectToBoundary(wp, area);
            if (result.empty() || distance(result.back(), snapped) > 0.01)
                result.push_back(snapped);
        }
    }
    return result;
}

struct Swath {
    Point a, b;
    bool visited;
};

Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg)
{
    Log(DBG, "%scalculateLinesPattern width=%.3f angle=%.1f", _LOG_, width, angleDeg);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    Polygon areaRotated = rotatePolygon(areaToMow, -angleDeg);
    double minX, minY, maxX, maxY;
    boundingBox(areaRotated, minX, minY, maxX, maxY);
    if (maxY - minY < 0.01) return {};

    // Create swath list (horizontal line segments clipped to areaToMow)
    std::vector<Swath> swaths;
    double y = minY;
    while (y <= maxY + 0.001) {
        auto crossings = intersectRayWithPolygon(y, areaRotated);
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            double x1 = crossings[i].x;
            double x2 = crossings[i + 1].x;
            if (std::abs(x2 - x1) < 0.02) continue;
            swaths.push_back({{x1, y}, {x2, y}, false});
        }
        y += width;
    }

    Log(DBG, "%scalculateLinesPattern %d swaths", _LOG_, swaths.size());
    if (swaths.empty()) return {};

    // Cassandra algorithm: greedy nearest-neighbor with path clearance check.
    // At each step pick the nearest unvisited swath endpoint that has a clear
    // direct line from currentPos.  If none is clear, fall back to nearest
    // regardless and use a perimeter walk to connect.
    Point start = perimeter.empty() ? Point{0,0} : rotatePoint(perimeter[0], -angleDeg);
    Polygon route;
    Point currentPos = start;

    auto addPoint = [&](const Point &p) {
        Point snapped = pointInPolygon(p, areaRotated) ? p : projectToBoundary(p, areaRotated);
        if (route.empty() || distance(route.back(), snapped) > 0.01)
            route.push_back(snapped);
    };

    auto isPathClear = [&](const Point &a, const Point &b) {
        if (areaRotated.size() < 3) return true;
        double d = distance(a, b);
        int steps = std::max(3, (int)(d / 0.15));
        for (int i = 1; i < steps; i++) {
            double t = (double)i / steps;
            Point p = {a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
            if (!pointInPolygon(p, areaRotated)) return false;
        }
        return true;
    };

    auto connectFallback = [&](const Point &target) {
        Polygon walk = walkPerimeter(areaRotated, currentPos, target);
        for (const auto &p : walk) addPoint(p);
    };

    // Assign levels to swaths based on their y-position (already sorted by construction)
    std::vector<int> swathLevel(swaths.size());
    for (size_t i = 0; i < swaths.size(); i++) {
        swathLevel[i] = (int)std::round((swaths[i].a.Y - minY) / width);
    }

    int currentLevel = -1;
    int remaining = (int)swaths.size();
    while (remaining > 0) {
        int bestIdx = -1;
        double bestDist = std::numeric_limits<double>::max();
        bool bestRev = false;
        bool needFallback = false;

        auto findBest = [&](bool restrictLevel) {
            int localBest = -1;
            double localBestDist = std::numeric_limits<double>::max();
            bool localBestRev = false;
            for (int i = 0; i < (int)swaths.size(); i++) {
                if (swaths[i].visited) continue;
                if (restrictLevel && currentLevel >= 0) {
                    int lvl = swathLevel[i];
                    if (std::abs(lvl - currentLevel) > 1) continue;
                }
                double dA = distance(currentPos, swaths[i].a);
                double dB = distance(currentPos, swaths[i].b);
                if (dA < localBestDist) { localBestDist = dA; localBest = i; localBestRev = false; }
                if (dB < localBestDist) { localBestDist = dB; localBest = i; localBestRev = true; }
            }
            return std::make_tuple(localBest, localBestDist, localBestRev);
        };

        // First pass: nearest swath in same/adjacent level with clear path
        {
            int localBest = -1;
            double localBestDist = std::numeric_limits<double>::max();
            bool localBestRev = false;
            for (int i = 0; i < (int)swaths.size(); i++) {
                if (swaths[i].visited) continue;
                if (currentLevel >= 0) {
                    int lvl = swathLevel[i];
                    if (std::abs(lvl - currentLevel) > 1) continue;
                }
                double dA = distance(currentPos, swaths[i].a);
                double dB = distance(currentPos, swaths[i].b);
                if (dA < localBestDist && isPathClear(currentPos, swaths[i].a)) {
                    localBestDist = dA; localBest = i; localBestRev = false;
                }
                if (dB < localBestDist && isPathClear(currentPos, swaths[i].b)) {
                    localBestDist = dB; localBest = i; localBestRev = true;
                }
            }
            if (localBest != -1) {
                bestIdx = localBest; bestDist = localBestDist; bestRev = localBestRev;
            }
        }

        // Second pass: nearest swath in any level with clear path
        if (bestIdx == -1) {
            for (int i = 0; i < (int)swaths.size(); i++) {
                if (swaths[i].visited) continue;
                double dA = distance(currentPos, swaths[i].a);
                double dB = distance(currentPos, swaths[i].b);
                if (dA < bestDist && isPathClear(currentPos, swaths[i].a)) {
                    bestDist = dA; bestIdx = i; bestRev = false;
                }
                if (dB < bestDist && isPathClear(currentPos, swaths[i].b)) {
                    bestDist = dB; bestIdx = i; bestRev = true;
                }
            }
        }

        // Fallback: no clear path to any swath → pick nearest regardless
        if (bestIdx == -1) {
            needFallback = true;
            // Try same/adjacent level first
            {
                int localBest = -1;
                double localBestDist = std::numeric_limits<double>::max();
                bool localBestRev = false;
                for (int i = 0; i < (int)swaths.size(); i++) {
                    if (swaths[i].visited) continue;
                    if (currentLevel >= 0) {
                        int lvl = swathLevel[i];
                        if (std::abs(lvl - currentLevel) > 1) continue;
                    }
                    double dA = distance(currentPos, swaths[i].a);
                    double dB = distance(currentPos, swaths[i].b);
                    if (dA < localBestDist) { localBestDist = dA; localBest = i; localBestRev = false; }
                    if (dB < localBestDist) { localBestDist = dB; localBest = i; localBestRev = true; }
                }
                if (localBest != -1) {
                    bestIdx = localBest; bestDist = localBestDist; bestRev = localBestRev;
                }
            }
            // Then any level
            if (bestIdx == -1) {
                for (int i = 0; i < (int)swaths.size(); i++) {
                    if (swaths[i].visited) continue;
                    double dA = distance(currentPos, swaths[i].a);
                    double dB = distance(currentPos, swaths[i].b);
                    if (dA < bestDist) { bestDist = dA; bestIdx = i; bestRev = false; }
                    if (dB < bestDist) { bestDist = dB; bestIdx = i; bestRev = true; }
                }
            }
        }

        if (bestIdx == -1) break;

        auto &sw = swaths[bestIdx];
        Point entry = bestRev ? sw.b : sw.a;

        if (needFallback)
            connectFallback(entry);

        addPoint(entry);
        addPoint(bestRev ? sw.a : sw.b);
        currentPos = route.back();
        sw.visited = true;
        currentLevel = swathLevel[bestIdx];
        remaining--;
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
        if (next.empty() || next.size() < 3) {
            // Polygon became too small for further offset
            break;
        }
        if (next.size() == currentArea.size() &&
            distance(next[0], currentArea[0]) < 0.001) {
            // offsetPolygonInward didn't actually change the polygon
            break;
        }
        double nextArea = std::abs(polygonArea(next));
        double curArea = std::abs(polygonArea(currentArea));
        if (nextArea >= curArea - 0.001) {
            // Area not decreasing (offset too large for this polygon)
            break;
        }
        if (nextArea < width * width * 0.5) {
            // Polygon became too small to continue meaningfully
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
    const Point &startNear, double width)
{
    Log(DBG, "%saddBorderLaps laps=%d ccw=%d width=%.3f", _LOG_, laps, ccw, width);
    if (laps <= 0 || perimeter.size() < 3) return {};

    Polygon route;
    Polygon currentRing = perimeter;

    for (int lap = 0; lap < laps; lap++) {
        // Orient ring according to ccw flag
        bool cw = isClockwise(currentRing);
        Polygon ring = currentRing;
        if (ccw == cw) std::reverse(ring.begin(), ring.end());

        // Start at nearest point
        size_t startIdx = nearestPointIndex(startNear, ring);
        Polygon ordered;
        for (size_t i = 0; i < ring.size(); i++) {
            size_t idx = (startIdx + i) % ring.size();
            if (ordered.empty() || distance(ordered.back(), ring[idx]) > 0.01)
                ordered.push_back(ring[idx]);
        }
        // Close ring
        if (!ordered.empty() && distance(ordered.back(), ordered[0]) > 0.01)
            ordered.push_back(ordered[0]);

        for (const auto &p : ordered)
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);

        // Shrink inward for next lap (Cassandra: buffer(-width))
        if (lap + 1 < laps) {
            Polygon next = offsetPolygonInward(currentRing, width);
            if (next.size() < 3) break;
            currentRing = next;
        }
    }

    Log(DBG, "%saddBorderLaps done: %d points", _LOG_, route.size());
    return route;
}

Polygon calculateWaypoints(MowerMap &map,
    MowSettings &settings)
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
