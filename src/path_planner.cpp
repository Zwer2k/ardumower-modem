#include "path_planner.h"
#include "log.h"
#include <cmath>
#include <algorithm>
#include <limits>

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

double crossProduct(const Point &a, const Point &b, const Point &c) {
    return (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X);
}

bool pointInPolygon(const Point &p, const Polygon &poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].Y > p.Y) != (poly[j].Y > p.Y) &&
            p.X < (poly[j].X - poly[i].X) * (p.Y - poly[i].Y) / (poly[j].Y - poly[i].Y) + poly[i].X) {
            inside = !inside;
        }
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
    for (const auto &p : poly)
        result.push_back(rotatePoint(p, angleDeg));
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
    double area = 0.0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        area += poly[i].X * poly[j].Y;
        area -= poly[j].X * poly[i].Y;
    }
    return area / 2.0;
}

bool isClockwise(const Polygon &poly) {
    return polygonArea(poly) < 0;
}

size_t nearestPointIndex(const Point &p, const Polygon &poly) {
    size_t best = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < poly.size(); i++) {
        double d = distance(p, poly[i]);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

std::vector<Intersection> intersectRayWithPolygon(double y, const Polygon &poly) {
    std::vector<Intersection> result;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        double y1 = poly[i].Y;
        double y2 = poly[j].Y;
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
    if (distance <= 0.001) return poly;
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
        if (bisectorLen < 0.001) {
            result.push_back({poly[i].X + nx1 * distance, poly[i].Y + ny1 * distance});
            continue;
        }
        bisectorX /= bisectorLen;
        bisectorY /= bisectorLen;
        double dot = nx1 * bisectorX + ny1 * bisectorY;
        if (std::abs(dot) < 0.001) continue;
        double offset = distance / dot;
        result.push_back({poly[i].X + bisectorX * offset, poly[i].Y + bisectorY * offset});
    }
    if (result.size() < 3) return poly;
    return result;
}

static Polygon orderSnakePattern(std::vector<Polygon> &segments, const Point &startNear) {
    Polygon route;
    if (segments.empty()) return route;
    size_t nearest = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < segments.size(); i++) {
        if (segments[i].empty()) continue;
        double d = distance(startNear, segments[i][0]);
        if (d < bestDist) {
            bestDist = d;
            nearest = i;
        }
        d = distance(startNear, segments[i].back());
        if (d < bestDist) {
            bestDist = d;
            nearest = i;
        }
    }
    std::swap(segments[0], segments[nearest]);
    bool reverse = distance(startNear, segments[0].back()) < distance(startNear, segments[0][0]);
    if (reverse) std::reverse(segments[0].begin(), segments[0].end());
    route.insert(route.end(), segments[0].begin(), segments[0].end());
    for (size_t i = 1; i < segments.size(); i++) {
        const Point &last = route.back();
        double dStart = distance(last, segments[i][0]);
        double dEnd = distance(last, segments[i].back());
        if (dEnd < dStart) {
            std::reverse(segments[i].begin(), segments[i].end());
        }
        route.insert(route.end(), segments[i].begin(), segments[i].end());
    }
    return route;
}

Polygon calculateLinesPattern(const Polygon &perimeter, const Polygon &areaToMow,
    double width, double angleDeg)
{
    Log(DBG, "%scalculateLinesPattern width=%.3f angle=%.1f", _LOG_, width, angleDeg);
    if (areaToMow.size() < 3 || width < 0.001) return {};

    Polygon rotated = rotatePolygon(areaToMow, -angleDeg);
    double minX, minY, maxX, maxY;
    boundingBox(rotated, minX, minY, maxX, maxY);

    std::vector<Polygon> segments;
    double y = minY;
    while (y <= maxY + 0.001) {
        auto crossings = intersectRayWithPolygon(y, rotated);
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            Polygon seg;
            seg.push_back({crossings[i].x, y});
            seg.push_back({crossings[i + 1].x, y});
            segments.push_back(seg);
        }
        y += width;
    }

    Log(DBG, "%scalculateLinesPattern %d line segments", _LOG_, segments.size());
    if (segments.empty()) return {};

    Point startNear = perimeter.empty() ? Point{0, 0} : perimeter[0];
    Polygon route = orderSnakePattern(segments, startNear);
    route = rotatePolygon(route, angleDeg);
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

    std::vector<Polygon> rings;
    Polygon currentRing = areaToMow;
    int maxRings = 50;
    for (int r = 0; r < maxRings; r++) {
        Polygon offset = offsetPolygonInward(currentRing, width);
        if (offset.size() < 3) {
            Polygon centroid;
            centroid.push_back({cx, cy});
            rings.push_back(centroid);
            break;
        }
        rings.push_back(offset);
        currentRing = offset;
    }

    Log(DBG, "%scalculateRingsPattern %d rings", _LOG_, rings.size());
    if (rings.empty()) return {};

    Polygon route;
    size_t startIdx = nearestPointIndex(perimeter.empty() ? Point{0, 0} : perimeter[0], rings[0]);
    for (size_t i = 0; i < rings[0].size(); i++) {
        size_t idx = (startIdx + i) % rings[0].size();
        route.push_back(rings[0][idx]);
    }
    for (size_t r = 1; r < rings.size(); r++) {
        if (rings[r].size() < 2) {
            route.push_back(rings[r][0]);
            continue;
        }
        size_t nearIdx = nearestPointIndex(route.back(), rings[r]);
        bool cw = isClockwise(rings[r]);
        if (cw) {
            for (size_t i = 0; i < rings[r].size(); i++) {
                size_t idx = (nearIdx + i) % rings[r].size();
                route.push_back(rings[r][idx]);
            }
        } else {
            for (int i = (int)rings[r].size() - 1; i >= 0; i--) {
                size_t idx = (nearIdx + i) % rings[r].size();
                route.push_back(rings[r][idx]);
            }
        }
    }

    Log(DBG, "%scalculateRingsPattern done: %d points", _LOG_, route.size());
    return route;
}

Polygon addBorderLaps(const Polygon &perimeter, int laps, bool ccw,
    const Point &startNear)
{
    Log(DBG, "%saddBorderLaps laps=%d ccw=%d", _LOG_, laps, ccw);
    if (laps <= 0 || perimeter.size() < 3) return {};

    Polygon route;
    Polygon border = perimeter;
    if (ccw != isClockwise(border)) {
        std::reverse(border.begin(), border.end());
    }

    for (int lap = 0; lap < laps; lap++) {
        Polygon ring;
        if (lap == 0) {
            ring = border;
        } else {
            ring = offsetPolygonInward(border, lap * 0.3);
            if (ring.size() < 3) break;
        }
        if (ring.empty()) continue;
        size_t startIdx = nearestPointIndex(startNear, ring);
        Polygon ordered;
        for (size_t i = 0; i < ring.size(); i++) {
            size_t idx = (startIdx + i) % ring.size();
            ordered.push_back(ring[idx]);
        }
        ordered.push_back(ordered[0]);
        route.insert(route.end(), ordered.begin(), ordered.end());
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
    for (const auto &p : map.perimeter)
        perimeter.push_back(p);

    if (perimeter.size() < 3) {
        Log(WARN, "%sPerimeter too small (%d)", _LOG_, perimeter.size());
        return {};
    }

    Polygon route;

    Point startNear = perimeter[0];

    if (settings.borderLaps > 0 && settings.mowBorderCcw) {
        Polygon borderLaps = addBorderLaps(perimeter, settings.borderLaps,
            settings.mowBorderCcw, startNear);
        route.insert(route.end(), borderLaps.begin(), borderLaps.end());
        if (!route.empty()) startNear = route.back();
    }

    Polygon areaToMow;
    if (settings.distanceToBorder > 0 && settings.width > 0.001) {
        double offsetDist = settings.distanceToBorder * settings.width;
        areaToMow = offsetPolygonInward(perimeter, offsetDist);
    } else {
        areaToMow = perimeter;
    }

    if (settings.mowArea && areaToMow.size() >= 3) {
        Polygon pattern;
        if (settings.pattern == 2) {
            pattern = calculateRingsPattern(perimeter, areaToMow, settings.width);
        } else if (settings.pattern == 1) {
            Polygon p1 = calculateLinesPattern(perimeter, areaToMow,
                settings.width, settings.angle);
            Polygon p2 = calculateLinesPattern(perimeter, areaToMow,
                settings.width, settings.angle + 90.0);
            pattern.reserve(p1.size() + p2.size());
            pattern.insert(pattern.end(), p1.begin(), p1.end());
            pattern.insert(pattern.end(), p2.begin(), p2.end());
        } else {
            pattern = calculateLinesPattern(perimeter, areaToMow,
                settings.width, settings.angle);
        }
        if (!route.empty() && !pattern.empty()) {
            route.push_back(route.back());
        }
        route.insert(route.end(), pattern.begin(), pattern.end());
    }

    if (settings.borderLaps > 0 && !settings.mowBorderCcw) {
        Polygon borderLaps = addBorderLaps(perimeter, settings.borderLaps,
            settings.mowBorderCcw,
            route.empty() ? startNear : route.back());
        if (!route.empty() && !borderLaps.empty()) {
            route.push_back(route.back());
        }
        route.insert(route.end(), borderLaps.begin(), borderLaps.end());
    }

    Log(INFO, "%scalculateWaypoints done: %d waypoints", _LOG_, route.size());
    return route;
}

        }
    }
}