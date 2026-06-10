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
    double sign = cw ? 1.0 : -1.0;
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

    // Generate horizontal sweep lines across the rotated polygon
    std::vector<Polygon> segments;
    double y = minY;
    while (y <= maxY + 0.001) {
        auto crossings = intersectRayWithPolygon(y, areaRotated);
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            double x1 = crossings[i].x;
            double x2 = crossings[i + 1].x;
            if (std::abs(x2 - x1) < 0.02) continue;
            Polygon seg;
            seg.push_back({x1, y});
            seg.push_back({x2, y});
            segments.push_back(seg);
        }
        y += width;
    }

    Log(DBG, "%scalculateLinesPattern %d segments", _LOG_, segments.size());
    if (segments.empty()) return {};

    // Sort segments by y-position for snake ordering
    std::sort(segments.begin(), segments.end(),
        [](const Polygon &a, const Polygon &b) {
            return a.empty() ? false : b.empty() ? true : a[0].Y < b[0].Y;
        });

    // Find nearest segment to start point (rotated)
    Point startNear = {0, 0};
    if (!perimeter.empty())
        startNear = rotatePoint(perimeter[0], -angleDeg);

    size_t nearest = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < segments.size(); i++) {
        if (segments[i].size() < 2) continue;
        for (int k = 0; k < 2; k++) {
            double d = distance(startNear, segments[i][k]);
            if (d < bestDist) { bestDist = d; nearest = i; }
        }
    }
    std::swap(segments[0], segments[nearest]);

    // Build route: each swath connected by walking the perimeter
    Polygon route;
    bool rev = distance(startNear, segments[0].back()) < distance(startNear, segments[0][0]);
    if (rev) std::reverse(segments[0].begin(), segments[0].end());
    for (const auto &p : segments[0])
        if (pointInPolygon(p, areaRotated))
            route.push_back(p);

    for (size_t i = 1; i < segments.size(); i++) {
        if (segments[i].size() < 2) continue;
        Point prevEnd = route.empty() ? startNear : route.back();
        double dS = distance(prevEnd, segments[i][0]);
        double dE = distance(prevEnd, segments[i].back());
        if (dE < dS) std::reverse(segments[i].begin(), segments[i].end());
        Point swathStart = segments[i][0];

        // Connect via perimeter walk instead of straight line
        double lineDist = distance(prevEnd, swathStart);
        if (lineDist > width * 2) {
            Polygon conn = walkPerimeter(periRotated, prevEnd, swathStart);
            for (const auto &p : conn)
                if (pointInPolygon(p, areaRotated) && distance(route.back(), p) > 0.01)
                    route.push_back(p);
        }
        for (const auto &p : segments[i])
            if (pointInPolygon(p, areaRotated) && distance(route.back(), p) > 0.01)
                route.push_back(p);
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

    double minX, minY, maxX, maxY;
    boundingBox(areaToMow, minX, minY, maxX, maxY);
    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    double maxR = std::sqrt((maxX - cx) * (maxX - cx) + (maxY - cy) * (maxY - cy));
    int numRings = std::max(1, (int)(maxR / width));

    Point center = {cx, cy};
    Polygon route;

    for (int r = 0; r < numRings; r++) {
        double scale = 1.0 - (double)(r + 1) / (numRings + 1);
        if (scale < 0.01) {
            if (route.empty() || distance(route.back(), center) > 0.01)
                route.push_back(center);
            break;
        }
        Polygon ring;
        ring.reserve(areaToMow.size());
        for (const auto &p : areaToMow) {
            double px = center.X + (p.X - center.X) * scale;
            double py = center.Y + (p.Y - center.Y) * scale;
            Point scaled = {px, py};
            if (pointInPolygon(scaled, areaToMow))
                ring.push_back(scaled);
        }
        if (ring.size() < 3) {
            if (route.empty() || distance(route.back(), center) > 0.01)
                route.push_back(center);
            break;
        }

        size_t startIdx = 0;
        if (!perimeter.empty())
            startIdx = nearestPointIndex(perimeter[0], ring);
        bool cw = isClockwise(ring);
        for (size_t i = 0; i < ring.size(); i++) {
            size_t idx = cw ? (startIdx + i) % ring.size() : (startIdx + ring.size() - i) % ring.size();
            route.push_back(ring[idx]);
        }
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