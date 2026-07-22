#include "pathplanner.h"
#include "clipper_adapter.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <queue>

#define _LOG_ "PathPlanner::"

// Default no-op logging for the standalone library. When compiled inside the
// firmware, this macro can be overridden by the build system to route logs to
// the actual logging framework.
#ifndef PP_LOG
#define PP_LOG(level, fmt, ...) do { (void)(level); (void)(fmt); } while(0)
#endif

namespace ArduMower {
namespace Modem {
namespace PathPlannerCore {

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

Polygon reversePolygon(const Polygon &poly) {
    Polygon rev = poly;
    std::reverse(rev.begin(), rev.end());
    return rev;
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

static bool segmentsIntersect(const Point &a1, const Point &a2, const Point &b1, const Point &b2) {
    double o1 = crossProduct(a1, a2, b1);
    double o2 = crossProduct(a1, a2, b2);
    double o3 = crossProduct(b1, b2, a1);
    double o4 = crossProduct(b1, b2, a2);
    if (o1 * o2 < 0 && o3 * o4 < 0) return true;
    // Check collinear cases
    auto onSegment = [](const Point &p, const Point &q, const Point &r) {
        return q.X <= std::max(p.X, r.X) + 1e-9 && q.X >= std::min(p.X, r.X) - 1e-9 &&
               q.Y <= std::max(p.Y, r.Y) + 1e-9 && q.Y >= std::min(p.Y, r.Y) - 1e-9;
    };
    if (std::abs(o1) < 1e-9 && onSegment(a1, b1, a2)) return true;
    if (std::abs(o2) < 1e-9 && onSegment(a1, b2, a2)) return true;
    if (std::abs(o3) < 1e-9 && onSegment(b1, a1, b2)) return true;
    if (std::abs(o4) < 1e-9 && onSegment(b1, a2, b2)) return true;
    return false;
}

static bool segmentIntersectsPolygon(const Point &from, const Point &to, const Polygon &poly) {
    if (poly.size() < 3) return false;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        if (segmentsIntersect(from, to, poly[i], poly[j])) return true;
    }
    return false;
}

static Point nearestPointOnPolygon(const Point &p, const Polygon &poly) {
    if (poly.size() < 3) return p;
    Point best = poly[0];
    double bestDist = distance(p, best);
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double dx = poly[j].X - poly[i].X, dy = poly[j].Y - poly[i].Y;
        double len2 = dx*dx + dy*dy;
        if (len2 < 1e-10) continue;
        double t = std::max(0.0, std::min(1.0, ((p.X - poly[i].X)*dx + (p.Y - poly[i].Y)*dy) / len2));
        Point proj = {poly[i].X + t*dx, poly[i].Y + t*dy};
        double d = distance(p, proj);
        if (d < bestDist) { bestDist = d; best = proj; }
    }
    return best;
}

static Polygon walkBoundaryWithHoles(const Point &from, const Point &to,
    const Polygon &outerBoundary,
    const std::vector<Polygon> &holes)
{
    // Primary route: graph-based Dijkstra over all boundaries (outer + holes).
    // If that fails to connect the two points, fall back to a simple outer-
    // boundary walk, which is always hole-free.
    std::vector<Polygon> boundaries;
    boundaries.push_back(outerBoundary);
    for (const auto &h : holes) {
        if (h.size() >= 3) boundaries.push_back(h);
    }

    struct Node { Point p; };
    std::vector<Node> nodes;
    std::vector<std::vector<std::pair<size_t, double>>> adj;
    std::vector<size_t> polyStart;

    for (const auto &poly : boundaries) {
        polyStart.push_back(nodes.size());
        for (size_t i = 0; i < poly.size(); i++) {
            nodes.push_back({poly[i]});
            adj.emplace_back();
        }
    }

    for (size_t b = 0; b < boundaries.size(); b++) {
        const auto &poly = boundaries[b];
        size_t start = polyStart[b];
        for (size_t i = 0; i < poly.size(); i++) {
            size_t j = (i + 1) % poly.size();
            double d = distance(nodes[start + i].p, nodes[start + j].p);
            adj[start + i].push_back({start + j, d});
            adj[start + j].push_back({start + i, d});
        }
    }

    auto project = [&](const Point &p, size_t &polyIdx, size_t &edgeIdx, Point &proj, double &bestDist) {
        polyIdx = std::numeric_limits<size_t>::max();
        edgeIdx = 0;
        proj = p;
        bestDist = std::numeric_limits<double>::max();
        for (size_t b = 0; b < boundaries.size(); b++) {
            const auto &poly = boundaries[b];
            for (size_t i = 0; i < poly.size(); i++) {
                size_t j = (i + 1) % poly.size();
                double dx = poly[j].X - poly[i].X;
                double dy = poly[j].Y - poly[i].Y;
                double len2 = dx * dx + dy * dy;
                if (len2 < 0.001) continue;
                double t = std::max(0.0, std::min(1.0, ((p.X - poly[i].X) * dx + (p.Y - poly[i].Y) * dy) / len2));
                Point pp = {poly[i].X + t * dx, poly[i].Y + t * dy};
                double dd = distance(p, pp);
                if (dd < bestDist) {
                    bestDist = dd;
                    polyIdx = b;
                    edgeIdx = i;
                    proj = pp;
                }
            }
        }
    };

    size_t fromPoly, fromEdge, toPoly, toEdge;
    Point fromProj, toProj;
    double fromDist, toDist;
    project(from, fromPoly, fromEdge, fromProj, fromDist);
    project(to, toPoly, toEdge, toProj, toDist);

    if (fromPoly == std::numeric_limits<size_t>::max() || toPoly == std::numeric_limits<size_t>::max()) {
        return {fromProj, toProj};
    }

    size_t fromNode = nodes.size();
    nodes.push_back({fromProj});
    adj.emplace_back();
    size_t toNode = nodes.size();
    nodes.push_back({toProj});
    adj.emplace_back();

    auto connectToEdge = [&](size_t node, size_t polyIdx, size_t edgeIdx) {
        size_t start = polyStart[polyIdx];
        const auto &poly = boundaries[polyIdx];
        size_t i = edgeIdx;
        size_t j = (edgeIdx + 1) % poly.size();
        double d1 = distance(nodes[node].p, nodes[start + i].p);
        double d2 = distance(nodes[node].p, nodes[start + j].p);
        adj[node].push_back({start + i, d1});
        adj[start + i].push_back({node, d1});
        adj[node].push_back({start + j, d2});
        adj[start + j].push_back({node, d2});
    };
    connectToEdge(fromNode, fromPoly, fromEdge);
    connectToEdge(toNode, toPoly, toEdge);

    std::vector<double> dist(nodes.size(), std::numeric_limits<double>::max());
    std::vector<int> prev(nodes.size(), -1);
    dist[fromNode] = 0.0;
    using PQItem = std::pair<double, size_t>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;
    pq.push({0.0, fromNode});
    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u] + 1e-9) continue;
        if (u == toNode) break;
        for (const auto &[v, w] : adj[u]) {
            if (dist[u] + w < dist[v] - 1e-9) {
                dist[v] = dist[u] + w;
                prev[v] = (int)u;
                pq.push({dist[v], v});
            }
        }
    }

    Polygon result;
    if (prev[toNode] != -1) {
        std::vector<size_t> path;
        for (int u = (int)toNode; u != -1; u = prev[u]) path.push_back((size_t)u);
        std::reverse(path.begin(), path.end());
        for (size_t idx : path) {
            if (result.empty() || distance(result.back(), nodes[idx].p) > 0.001)
                result.push_back(nodes[idx].p);
        }
        return result;
    }

    // Fallback: walk along the outer boundary between the two projections.
    Point fromOuter = nearestPointOnPolygon(from, outerBoundary);
    Point toOuter = nearestPointOnPolygon(to, outerBoundary);

    size_t iFrom = nearestPointIndex(fromOuter, outerBoundary);
    size_t iTo = nearestPointIndex(toOuter, outerBoundary);
    size_t n = outerBoundary.size();

    if (iFrom == iTo || distance(fromOuter, toOuter) < 0.001) {
        return {fromOuter, toOuter};
    }

    auto buildPath = [&](size_t startPt, size_t endPt) -> Polygon {
        Polygon path;
        path.push_back(fromOuter);
        for (size_t k = startPt; ; k = (k + 1) % n) {
            path.push_back(outerBoundary[k]);
            if (k == endPt) break;
            if (path.size() > n + 3) break;
        }
        path.push_back(toOuter);
        return path;
    };

    Polygon cwPath = buildPath((iFrom + 1) % n, iTo);
    Polygon ccwPath = buildPath((iTo + 1) % n, iFrom);

    double cwLen = 0, ccwLen = 0;
    for (size_t i = 1; i < cwPath.size(); i++) cwLen += distance(cwPath[i-1], cwPath[i]);
    for (size_t i = 1; i < ccwPath.size(); i++) ccwLen += distance(ccwPath[i-1], ccwPath[i]);

    return cwLen < ccwLen ? cwPath : ccwPath;
}

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

static Point projectToAreaBoundary(const Point &p, const std::vector<Polygon> &areas) {
    Point best = p;
    double bestDist = std::numeric_limits<double>::max();
    bool insideAny = false;
    for (const auto &area : areas) {
        if (area.size() < 3) continue;
        if (pointInPolygon(p, area)) { insideAny = true; break; }
        Point proj = projectToBoundary(p, area);
        double d = distance(p, proj);
        if (d < bestDist) { bestDist = d; best = proj; }
    }
    if (insideAny) return p;
    return best;
}

static Polygon pruneOutside(const Polygon &waypoints, const std::vector<Polygon> &areas) {
    if (areas.empty()) return waypoints;
    Polygon result;
    result.reserve(waypoints.size());
    for (const auto &wp : waypoints) {
        Point snapped = projectToAreaBoundary(wp, areas);
        if (result.empty() || distance(result.back(), snapped) > 0.01)
            result.push_back(snapped);
    }
    return result;
}

static Polygon pruneOutside(const Polygon &waypoints, const Polygon &area) {
    return pruneOutside(waypoints, std::vector<Polygon>{area});
}

static bool lineExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter);
static bool segmentExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter);

Polygon calculateRingsPattern(const Polygon &perimeter, const Polygon &areaToMow,
    const std::vector<Polygon> &holes, double width, const Point &startNear)
{
    (void)perimeter;
    PP_LOG(0, "%scalculateRingsPattern width=%.3f holes=%d", _LOG_, width, (int)holes.size());
    if (areaToMow.size() < 3 || width < 0.001) return {};

    // Use a very small margin so rings stay as close as possible to triangular
    // exclusions while still not touching them.
    const double holeMargin = width * 0.1;

    auto shrinkByHoles = [&](const Polygon &ring, double margin) -> std::vector<Polygon> {
        if (holes.empty()) return {ring};
        std::vector<Polygon> inflatedHoles;
        for (const auto &h : holes) {
            if (h.size() < 3) continue;
            Polygon positive = (polygonArea(h) > 0.0) ? h : reversePolygon(h);
            auto expanded = clipOffset(positive, margin);
            for (const auto &p : expanded) inflatedHoles.push_back(p);
        }
        auto clipped = clipDifference(ring, inflatedHoles);
        std::vector<Polygon> result;
        for (const auto &p : clipped) {
            if (p.size() >= 3 && polygonArea(p) > 0.001)
                result.push_back(p);
        }
        return result;
    };

    auto clipRingAgainstHoles = [&](const Polygon &ring) -> std::vector<Polygon> {
        return shrinkByHoles(ring, holeMargin);
    };

    // Start the ring sequence from a slightly shrunken area so that the first
    // ring never grazes the exclusion boundary when the user does not want to
    // mow along it.  This keeps the outermost ring clearly inside the mowable
    // region.
    Polygon first = areaToMow;
    if (!holes.empty()) {
        auto shrunk = shrinkByHoles(first, holeMargin);
        if (!shrunk.empty()) {
            first = shrunk[0];
            for (size_t i = 1; i < shrunk.size(); i++)
                if (polygonArea(shrunk[i]) > polygonArea(first))
                    first = shrunk[i];
        }
    }
    size_t firstBest = nearestPointIndex(startNear, first);
    Polygon firstRot;
    firstRot.reserve(first.size());
    for (size_t i = 0; i < first.size(); i++)
        firstRot.push_back(first[(firstBest + i) % first.size()]);

    std::vector<Polygon> queue;
    queue.push_back(firstRot);

    std::vector<Polygon> rings;

    while (!queue.empty()) {
        Polygon currentArea = queue.front();
        queue.erase(queue.begin());
        if (currentArea.size() < 3) continue;

        if (!rings.empty()) {
            const Polygon &lastRing = rings.back();
            size_t best = nearestPointIndex(lastRing.back(), currentArea);
            Polygon rot;
            rot.reserve(currentArea.size());
            for (size_t i = 0; i < currentArea.size(); i++)
                rot.push_back(currentArea[(best + i) % currentArea.size()]);
            currentArea = rot;
        }

        rings.push_back(currentArea);

        auto nextList = clipOffset(currentArea, -width);
        double curAreaVal = std::abs(polygonArea(currentArea));
        for (const auto &next : nextList) {
            if (next.size() < 3) continue;
            double nextAreaVal = std::abs(polygonArea(next));
            if (nextAreaVal < 0.001 || nextAreaVal >= curAreaVal - 0.001) continue;
            auto clipped = clipRingAgainstHoles(next);
            for (const auto &c : clipped) queue.push_back(c);
        }
    }

    Polygon route;
    for (size_t i = 0; i < rings.size(); i++) {
        if (i > 0) {
            Point from = route.back();
            Point to = rings[i][0];
            if (distance(from, to) > 0.001) {
                // If the direct connector crosses an exclusion or leaves the
                // perimeter, route along the original perimeter boundary
                // instead of using the straight line. This keeps the mower
                // inside the mapped area even between distant ring start/end
                // points.
                bool crossesHole = false;
                for (const auto &hole : holes) {
                    if (segmentIntersectsPolygon(from, to, hole)) {
                        crossesHole = true;
                        break;
                    }
                }
                bool exitsPerimeter = lineExitsPerimeter(from, to, perimeter);
                if (crossesHole || exitsPerimeter) {
                    Polygon conn = walkBoundaryWithHoles(from, to, perimeter, holes);
                    for (const auto &p : conn) {
                        if (route.empty() || distance(route.back(), p) > 0.01)
                            route.push_back(p);
                    }
                } else {
                    route.push_back(from);
                    route.push_back(to);
                }
            }
        }
        for (const auto &p : rings[i])
            if (route.empty() || distance(route.back(), p) > 0.01)
                route.push_back(p);
    }

    PP_LOG(0, "%scalculateRingsPattern done: %d points", _LOG_, route.size());
    return route;
}

Polygon addBorderLaps(const Polygon &perimeter, const std::vector<Polygon> &holes,
    int laps, bool ccw, const Point &startNear, double width)
{
    PP_LOG(0, "%saddBorderLaps laps=%d ccw=%d width=%.3f", _LOG_, laps, ccw, width);
    if (laps <= 0 || perimeter.size() < 3) return {};

    Polygon route;
    Point near = startNear;
    Polygon currentBoundary = perimeter;

    for (int lap = 0; lap < laps; lap++) {
        // If we are supposed to mow exclusion borders, the lap boundary is the
        // outer perimeter with all exclusions cut out.  Offsetting this inward
        // then gives us separate polygons: one for the outer perimeter and one
        // for each exclusion, all at the correct lap distance.
        std::vector<Polygon> boundaries;
        if (!holes.empty()) {
            auto diff = clipDifference(currentBoundary, holes);
            for (const auto &p : diff) {
                if (p.size() >= 3) boundaries.push_back(p);
            }
        }
        if (boundaries.empty()) boundaries.push_back(currentBoundary);

        // Sort boundaries so we start each lap from the closest polygon and
        // continue with the nearest remaining polygon.  This produces a
        // continuous-ish lap route that stays within the mowable area.
        sortSolutionPolygonsByDistance(boundaries, near);

        for (size_t b = 0; b < boundaries.size(); b++) {
            Polygon ring = boundaries[b];
            bool cw = isClockwise(ring);
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
        }

        // Prepare the next lap boundary by shrinking the current outer
        // perimeter inward.  Holes stay fixed so the inner lap polygons keep the
        // correct shape around the exclusions.
        if (lap + 1 < laps) {
            auto nextList = clipOffset(currentBoundary, -width);
            if (nextList.empty() || nextList[0].size() < 3) break;
            currentBoundary = nextList[0];
        }
    }

    PP_LOG(0, "%saddBorderLaps done: %d points", _LOG_, route.size());
    return route;
}

void sortSolutionPolygonsByDistance(std::vector<Polygon> &solution, const Point &startPt) {
    std::vector<Polygon> sorted;
    sorted.reserve(solution.size());
    Point currentPos = startPt;

    while (!solution.empty()) {
        double minDist = 160000;
        size_t minPolyIdx = 0;
        bool reverse = false;

        for (size_t i = 0; i < solution.size(); i++) {
            size_t n = solution[i].size();
            if (n == 0) continue;

            double dFirst = distance(currentPos, solution[i][0]);
            double dLast = distance(currentPos, solution[i][n - 1]);
            if (dFirst < minDist) {
                minDist = dFirst; minPolyIdx = i; reverse = false;
            }
            if (dLast < minDist) {
                minDist = dLast; minPolyIdx = i; reverse = true;
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

static void addEdgeEvents(const Point &a, const Point &b,
    const Point &c, const Point &d, std::vector<double> &ts)
{
    double dx1 = b.X - a.X, dy1 = b.Y - a.Y;
    double dx2 = d.X - c.X, dy2 = d.Y - c.Y;
    double denom = dx1 * dy2 - dy1 * dx2;

    auto proj = [&](const Point &p) -> double {
        double len2 = dx1 * dx1 + dy1 * dy1;
        if (len2 < 1e-14) return 0.0;
        return ((p.X - a.X) * dx1 + (p.Y - a.Y) * dy1) / len2;
    };

    if (std::abs(denom) < 1e-12) {
        double cross = (c.X - a.X) * dy1 - (c.Y - a.Y) * dx1;
        if (std::abs(cross) > 1e-9) return; // parallel, not collinear
        double tC = proj(c);
        double tD = proj(d);
        double t1 = std::max(0.0, std::min(tC, tD));
        double t2 = std::min(1.0, std::max(tC, tD));
        if (t2 > t1 + 1e-9) {
            ts.push_back(t1);
            ts.push_back(t2);
        }
        return;
    }

    double t = ((c.X - a.X) * dy2 - (c.Y - a.Y) * dx2) / denom;
    double u = ((c.X - a.X) * dy1 - (c.Y - a.Y) * dx1) / denom;
    if (t < -1e-9 || t > 1.0 + 1e-9 || u < -1e-9 || u > 1.0 + 1e-9) return;
    if (t >= 0.0 && t <= 1.0) ts.push_back(t);
}

std::vector<Polygon> clipSegmentsAgainstHoles(const std::vector<Polygon> &segments,
    const std::vector<Polygon> &holes)
{
    std::vector<Polygon> result;
    for (const auto &seg : segments) {
        if (seg.size() < 2) continue;
        for (size_t i = 0; i + 1 < seg.size(); i++) {
            const Point &a = seg[i];
            const Point &b = seg[i + 1];
            if (distance(a, b) < 1e-9) continue;

            std::vector<double> ts;
            for (const auto &hole : holes) {
                if (hole.size() < 3) continue;
                for (size_t j = 0; j < hole.size(); j++) {
                    size_t k = (j + 1) % hole.size();
                    addEdgeEvents(a, b, hole[j], hole[k], ts);
                }
                if (pointInPolygon(a, hole) || pointOnBoundary(a, hole, 1e-6)) ts.push_back(0.0);
                if (pointInPolygon(b, hole) || pointOnBoundary(b, hole, 1e-6)) ts.push_back(1.0);
            }

            if (ts.empty()) {
                Point mid = lerp(a, b, 0.5);
                bool inHole = false;
                for (const auto &hole : holes) {
                    if (pointInPolygon(mid, hole) || pointOnBoundary(mid, hole, 1e-6)) {
                        inHole = true; break;
                    }
                }
                if (!inHole) result.push_back({a, b});
                continue;
            }

            std::sort(ts.begin(), ts.end());
            std::vector<double> uniq;
            for (double t : ts) {
                if (uniq.empty() || t > uniq.back() + 1e-9) uniq.push_back(t);
            }

            std::vector<std::pair<double, double>> insideIntervals;
            for (size_t j = 0; j + 1 < uniq.size(); j++) {
                if (uniq[j + 1] - uniq[j] < 1e-9) continue;
                double tmid = (uniq[j] + uniq[j + 1]) * 0.5;
                Point pmid = lerp(a, b, tmid);
                bool inHole = false;
                for (const auto &hole : holes) {
                    if (pointInPolygon(pmid, hole) || pointOnBoundary(pmid, hole, 1e-6)) {
                        inHole = true; break;
                    }
                }
                if (inHole) insideIntervals.push_back({uniq[j], uniq[j + 1]});
            }

            std::sort(insideIntervals.begin(), insideIntervals.end());
            std::vector<std::pair<double, double>> merged;
            for (const auto &iv : insideIntervals) {
                if (merged.empty() || iv.first > merged.back().second + 1e-9) {
                    merged.push_back(iv);
                } else if (iv.second > merged.back().second) {
                    merged.back().second = iv.second;
                }
            }

            double last = 0.0;
            for (const auto &iv : merged) {
                if (iv.first > last + 1e-9)
                    result.push_back({lerp(a, b, last), lerp(a, b, iv.first)});
                last = std::max(last, iv.second);
            }
            if (last < 1.0 - 1e-9)
                result.push_back({lerp(a, b, last), b});
        }
    }
    return result;
}

static bool lineExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter) {
    for (size_t i = 0; i < perimeter.size(); i++) {
        size_t j = (i + 1) % perimeter.size();
        double o1 = crossProduct(perimeter[i], perimeter[j], from);
        double o2 = crossProduct(perimeter[i], perimeter[j], to);
        double o3 = crossProduct(from, to, perimeter[i]);
        double o4 = crossProduct(from, to, perimeter[j]);
        if (o1 * o2 < 0 && o3 * o4 <= 0) return true;
    }

    double segLen = distance(from, to);
    if (segLen > 1e-6) {
        double step = 0.10;
        int samples = std::max(1, (int)(segLen / step));
        if (samples > 20) samples = 20;
        for (int k = 1; k < samples; k++) {
            double t = (double)k / samples;
            Point p{from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t};
            if (!pointInPolygon(p, perimeter) && !pointOnBoundary(p, perimeter))
                return true;
        }
    }
    return false;
}

static bool segmentExitsPerimeter(const Point &from, const Point &to, const Polygon &perimeter) {
    double segLen = distance(from, to);
    if (segLen < 1e-6) return false;
    int samples = std::max(5, (int)(segLen / 0.005));
    if (samples > 200) samples = 200;
    for (int k = 0; k <= samples; k++) {
        double t = (double)k / samples;
        Point p{from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t};
        if (!pointInPolygon(p, perimeter) && !pointOnBoundary(p, perimeter, 1e-6))
            return true;
    }
    return false;
}

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
            if (pointInPolygon(p, area) || pointOnBoundary(p, area, 1e-6)) {
                insideAny = true;
                break;
            }
        }
        if (!insideAny) return false;
    }
    return true;
}

void connectPolysUsingPathFinding(Polygon &waypoints, const std::vector<Polygon> &polys,
    const Polygon &perimeter, const std::vector<Polygon> &areasToMow,
    const std::vector<Polygon> &holes) {
    waypoints.clear();

    for (size_t i = 0; i < polys.size(); i++) {
        const auto &poly = polys[i];
        if (poly.empty()) continue;

        if (i > 0) {
            const Point &from = waypoints.back();
            const Point &to = poly[0];
            double d = distance(from, to);
            // Always check against the original perimeter so a connector can
            // never leave the mapped area, even when areasToMow is inset.
            bool exitsPerimeter = d > 0.001 && segmentExitsPerimeter(from, to, perimeter);
            bool leavesMowArea = d > 0.001 && !segmentStaysInAreas(from, to, areasToMow);
            bool crossesHole = false;
            if (!holes.empty()) {
                std::vector<Polygon> connSeg = clipSegmentsAgainstHoles({{from, to}}, holes);
                if (connSeg.empty() || connSeg[0].size() < 2 ||
                    distance(connSeg[0][0], connSeg[0].back()) < d - 1e-6) {
                    crossesHole = true;
                }
                // Additional robustness: segment midpoint or sample points may
                // be inside a hole even when endpoints are outside and the
                // direct line stays mostly outside the hole boundary.
                if (!crossesHole) {
                    int samples = std::max(3, (int)(d / 0.05));
                    if (samples > 20) samples = 20;
                    for (int k = 1; k < samples && !crossesHole; k++) {
                        double t = (double)k / samples;
                        Point p{from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t};
                        for (const auto &hole : holes) {
                            if (pointInPolygon(p, hole)) {
                                crossesHole = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (exitsPerimeter || leavesMowArea || crossesHole) {
                // Route along the original perimeter boundary (and holes) so
                // the connector stays inside the mapped area.
                Polygon conn = walkBoundaryWithHoles(from, to, perimeter, holes);
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

Polygon calculateWaypoints(Map &map, Settings &settings, const State *state) {
    PP_LOG(0, "%scalculateWaypoints pattern=%d width=%.3f angle=%d distToBorder=%d borderLaps=%d",
        _LOG_, settings.pattern, settings.width, settings.angle,
        settings.distanceToBorder, settings.borderLaps);

    Polygon perimeter = map.perimeter;
    if (perimeter.size() < 3) {
        PP_LOG(0, "%sPerimeter too small", _LOG_);
        return {};
    }

    Polygon route;
    Point startNear = perimeter[0];

    if (state != nullptr) {
        const int JOB_DOCK = 4;
        if (state->job == JOB_DOCK && !map.dockpoints.empty()) {
            const auto &dp = map.dockpoints.back();
            startNear = Point{dp.X, dp.Y};
            PP_LOG(0, "%sstartNear set to dock line end (%.3f, %.3f)", _LOG_, startNear.X, startNear.Y);
        } else if (state->position.solution > 0 &&
                   (state->position.x != 0.0 || state->position.y != 0.0)) {
            startNear = Point{state->position.x, state->position.y};
            PP_LOG(0, "%sstartNear set to GPS position (%.3f, %.3f)", _LOG_, startNear.X, startNear.Y);
        }
    }

    std::vector<Polygon> areasToMow;
    std::vector<Polygon> mowableRegion; // outer boundary + holes used for clipping
    if (map.exclusions.empty()) {
      if (settings.distanceToBorder > 0 && settings.width > 0.001) {
        double offsetDist = settings.distanceToBorder * settings.width;
        areasToMow = offsetPolygonInward(perimeter, offsetDist);
      }
      mowableRegion = areasToMow.empty() ? std::vector<Polygon>{perimeter} : areasToMow;
    } else {
      // If the user does not want to mow the exclusion border, keep the
      // mowable area at a small safety distance from each exclusion so
      // generated rings/lines never graze the exclusion boundary.
      std::vector<Polygon> effectiveExclusions = map.exclusions;
      if (!settings.mowExclusionBorder) {
        double exclusionMargin = settings.width * 0.5;
        if (exclusionMargin < 1e-3) exclusionMargin = 1e-3;
        std::vector<Polygon> expanded;
        for (const auto &excl : map.exclusions) {
          if (excl.size() < 3) continue;
          auto e = clipOffset(excl, exclusionMargin);
          for (const auto &p : e) {
            if (polygonArea(p) > 0.0) expanded.push_back(p);
            else expanded.push_back(reversePolygon(p));
          }
        }
        if (!expanded.empty()) effectiveExclusions = std::move(expanded);
      }
      mowableRegion = clipDifference(perimeter, effectiveExclusions);
      if (mowableRegion.empty()) {
        PP_LOG(0, "%sExclusions removed entire perimeter, nothing to mow", _LOG_);
        return {};
      }
      if (settings.distanceToBorder > 0 && settings.width > 0.001) {
        double offsetDist = settings.distanceToBorder * settings.width;
        for (const auto &area : mowableRegion) {
          if (polygonArea(area) < 0.0) continue; // skip holes
          auto offset = offsetPolygonInward(area, offsetDist);
          areasToMow.insert(areasToMow.end(), offset.begin(), offset.end());
        }
      } else {
        for (const auto &area : mowableRegion)
          if (polygonArea(area) > 0.0) areasToMow.push_back(area);
      }
    }
    if (areasToMow.empty()) areasToMow = {perimeter};
    for (auto it = areasToMow.begin(); it != areasToMow.end(); )
        if (it->size() < 3 || polygonArea(*it) < 0.0) it = areasToMow.erase(it); else ++it;
    if (areasToMow.empty()) return {};

    // Collect exclusion holes for connector checks (negative-area polygons in mowableRegion).
    // Inflate holes slightly so clipped segment endpoints lie outside the original exclusion.
    std::vector<Polygon> holes;
    for (const auto &poly : mowableRegion)
        if (polygonArea(poly) < 0.0) holes.push_back(poly);
    {
        std::vector<Polygon> inflatedHoles;
        double inflateBy = settings.width * 0.05;
        if (inflateBy < 1e-3) inflateBy = 1e-3;
        for (const auto &hole : holes) {
            auto expanded = clipOffset(reversePolygon(hole), inflateBy);
            for (auto &p : expanded) {
                if (polygonArea(p) < 0.0) inflatedHoles.push_back(p);
                else inflatedHoles.push_back(reversePolygon(p));
            }
        }
        holes = std::move(inflatedHoles);
    }

    if (settings.borderLaps > 0 && settings.mowBorderCcw) {
        std::vector<Polygon> borderHoles;
        if (settings.mowExclusionBorder) {
            for (const auto &ex : map.exclusions) {
                if (ex.size() >= 3) borderHoles.push_back(ex);
            }
        }
        Polygon borderLaps = addBorderLaps(perimeter, borderHoles, settings.borderLaps, true, startNear, settings.width);
        route = borderLaps;
        if (!route.empty()) startNear = route.back();
    }

    if (settings.mowArea) {
        std::vector<Polygon> allSegments;

        if (settings.pattern == 2) {
            for (const auto &area : areasToMow) {
                Polygon ar = calculateRingsPattern(perimeter, area, holes, settings.width, startNear);
                if (!ar.empty()) allSegments.push_back(ar);
            }
        } else {
            int passes = (settings.pattern == 1) ? 2 : 1;
            for (int pass = 0; pass < passes; pass++) {
                double angleDeg = settings.angle + (pass == 1 ? 90.0 : 0.0);

                auto rotatedAreas = rotatePolygons(areasToMow, -angleDeg);
                auto rotatedMowableRegion = rotatePolygons(mowableRegion, -angleDeg);

                // Identify the outer boundary of the mowable region.
                // Clipper2 Difference returns holes as separate negative-area polygons,
                // but we already collected (and inflated) them above for clipping.
                Polygon outerBoundary;
                for (const auto &poly : rotatedMowableRegion) {
                    if (polygonArea(poly) > 0.0) outerBoundary = poly;
                }
                if (outerBoundary.empty() && !rotatedMowableRegion.empty())
                    outerBoundary = rotatedMowableRegion[0];

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

                double margin = settings.width * 2;
                double xRange = std::max(std::abs(rMinX), std::abs(rMaxX)) + margin;
                double lastX = -xRange;
                Polygon zigzag;
                for (double y = rMinY - margin; y <= rMaxY + margin; y += settings.width) {
                    zigzag.push_back({lastX, y});
                    zigzag.push_back({-lastX, y});
                    lastX = -lastX;
                }

                // Rotate the already-inflated holes into the pass coordinate system.
                std::vector<Polygon> rotatedHoles = rotatePolygons(holes, -angleDeg);

                // Clip open zigzag against the mowable areas (already
                // exclusion-free). This is more robust than clipping against
                // the outer boundary and then subtracting holes separately,
                // because some long segments may pass through the hole if the
                // open-path intersection does not split them correctly.
                auto clipped = clipIntersectOpen({zigzag}, rotatedAreas);
                // Keep the hole subtraction as a safety net for any segments
                // that still cross an inflated hole.
                if (!rotatedHoles.empty()) {
                    clipped = clipSegmentsAgainstHoles(clipped, rotatedHoles);
                }
                auto passSegments = rotatePolygons(clipped, angleDeg);

                if (!passSegments.empty()) {
                    sortSolutionPolygonsByDistance(passSegments, startNear);
                    Polygon passRoute;
                    connectPolysUsingPathFinding(passRoute, passSegments, perimeter, areasToMow, holes);
                    passRoute = pruneOutside(passRoute, areasToMow);
                    if (!passRoute.empty()) allSegments.push_back(passRoute);
                }
            }
        }

        if (!allSegments.empty()) {
            sortSolutionPolygonsByDistance(allSegments, startNear);
            Polygon pattern;
            connectPolysUsingPathFinding(pattern, allSegments, perimeter, areasToMow, holes);
            pattern = pruneOutside(pattern, areasToMow);

            if (!route.empty() && !pattern.empty()) {
                Polygon conn = walkBoundaryWithHoles(route.back(), pattern[0], perimeter, holes);
                for (size_t k = 0; k < conn.size(); k++)
                    if (distance(route.back(), conn[k]) > 0.01)
                        route.push_back(conn[k]);
            }
            route.insert(route.end(), pattern.begin(), pattern.end());
            if (!route.empty()) startNear = route.back();
        }
    }

    if (settings.borderLaps > 0 && !settings.mowBorderCcw) {
        std::vector<Polygon> borderHoles;
        if (settings.mowExclusionBorder) {
            for (const auto &ex : map.exclusions) {
                if (ex.size() >= 3) borderHoles.push_back(ex);
            }
        }
        Polygon borderLaps = addBorderLaps(perimeter, borderHoles, settings.borderLaps, false, startNear, settings.width);
        if (!route.empty() && !borderLaps.empty()) {
            Polygon conn = walkPerimeter(perimeter, route.back(), borderLaps[0]);
            for (size_t k = 0; k < conn.size(); k++)
                if (distance(route.back(), conn[k]) > 0.01)
                    route.push_back(conn[k]);
        }
        route.insert(route.end(), borderLaps.begin(), borderLaps.end());
    }

    route = pruneOutside(route, perimeter);

    // Final safety pass: ensure no route segment leaves the perimeter.
    // This can happen for connector segments produced by different pattern
    // stages (border laps, rings, zigzag) when the direct line cuts across a
    // concave part of the perimeter or across exclusion holes.
    Polygon safeRoute;
    for (size_t i = 0; i < route.size(); i++) {
        if (i == 0) {
            safeRoute.push_back(route[i]);
            continue;
        }
        const Point &from = safeRoute.back();
        const Point &to = route[i];
        if (distance(from, to) < 0.001) continue;

        if (segmentExitsPerimeter(from, to, perimeter)) {
            Polygon conn = walkBoundaryWithHoles(from, to, perimeter, holes);
            for (const auto &p : conn) {
                if (safeRoute.empty() || distance(safeRoute.back(), p) > 0.01)
                    safeRoute.push_back(p);
            }
        }
        safeRoute.push_back(to);
    }
    route = std::move(safeRoute);

    PP_LOG(0, "%scalculateWaypoints done: %d waypoints", _LOG_, route.size());
    return route;
}

} // namespace PathPlannerCore
} // namespace Modem
} // namespace ArduMower
