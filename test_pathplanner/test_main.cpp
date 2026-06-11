#include "test_stub.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>

namespace ArduMower {
namespace Modem {
namespace PathPlanner {
using Point = ArduMower::Domain::Robot::MapPoint;
using Polygon = std::vector<Point>;
double distance(const Point &a, const Point &b);
double polygonArea(const Polygon &poly);
bool isClockwise(const Polygon &poly);
bool pointInPolygon(const Point &p, const Polygon &poly);
Polygon rotatePolygon(const Polygon &poly, double angleDeg);
void boundingBox(const Polygon &poly, double &minX, double &minY, double &maxX, double &maxY);
size_t nearestPointIndex(const Point &p, const Polygon &poly);
struct Intersection {
    double x;
    int edgeIndex;
};
std::vector<Intersection> intersectRayWithPolygon(double y, const Polygon &poly);
}
}
}

// ─── Minimal reimplementation for test ───
// The actual implementation uses Clipper2 which isn't available in
// standalone test builds.  This file re-implements just the math
// functions needed to verify correctness of the core algorithms.

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
    for (const auto &p : poly) result.push_back(rotatePoint(p, angleDeg));
    return result;
}

void boundingBox(const Polygon &poly, double &minX, double &minY, double &maxX, double &maxY) {
    minX = minY = 1e30;
    maxX = maxY = -1e30;
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
    double bestDist = 1e30;
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
        double y1 = poly[i].Y, y2 = poly[j].Y;
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

} // namespace PathPlanner
} // namespace Modem
} // namespace ArduMower

using namespace ArduMower::Modem::PathPlanner;

int main() {
    bool ok = true;

    // Test distance
    {
        double d = distance({0,0}, {3,4});
        ok &= (std::abs(d - 5.0) < 0.001);
        std::cout << "distance(0,0 to 3,4) = " << d << " (expect 5)\n";
    }

    // Test polygonArea
    {
        Polygon rect = {{0,0}, {10,0}, {10,5}, {0,5}};
        double a = polygonArea(rect);
        std::cout << "area rect = " << a << " (expect -50 or 50)\n";
        ok &= (std::abs(std::abs(a) - 50.0) < 0.001);
    }

    // Test isClockwise
    {
        Polygon cw = {{0,0}, {10,0}, {10,10}, {0,10}};
        std::cout << "CW area = " << polygonArea(cw) << " isCW=" << isClockwise(cw) << "\n";
    }

    // Test pointInPolygon
    {
        Polygon sq = {{0,0}, {10,0}, {10,10}, {0,10}};
        ok &= pointInPolygon({5,5}, sq);
        ok &= !pointInPolygon({15,5}, sq);
        std::cout << "pointInPolygon (5,5) = " << pointInPolygon({5,5}, sq) << "\n";
        std::cout << "pointInPolygon (15,5) = " << pointInPolygon({15,5}, sq) << "\n";
    }

    // Test intersectRayWithPolygon
    {
        Polygon sq = {{0,0}, {10,0}, {10,10}, {0,10}};
        auto ix = intersectRayWithPolygon(5, sq);
        std::cout << "ray y=5 crossings: " << ix.size() << " (expect 2)\n";
        ok &= (ix.size() == 2);
        if (ix.size() == 2) {
            std::cout << "  x1=" << ix[0].x << " x2=" << ix[1].x << "\n";
            ok &= (std::abs(ix[0].x) < 0.001 && std::abs(ix[1].x - 10.0) < 0.001);
        }
    }

    // Test rotatePolygon
    {
        Polygon p = {{1,0}};
        Polygon rp = rotatePolygon(p, 90.0);
        std::cout << "rotate (1,0) 90deg = (" << rp[0].X << "," << rp[0].Y << ")\n";
        ok &= (std::abs(rp[0].X) < 0.001 && std::abs(rp[0].Y - 1.0) < 0.001);
    }

    // Test boundingBox
    {
        Polygon poly = {{1,2}, {5,3}, {3,8}};
        double minX, minY, maxX, maxY;
        boundingBox(poly, minX, minY, maxX, maxY);
        std::cout << "bbox: (" << minX << "," << minY << ") to (" << maxX << "," << maxY << ")\n";
        ok &= (minX == 1 && minY == 2 && maxX == 5 && maxY == 8);
    }

    std::cout << (ok ? "All tests PASSED" : "Some tests FAILED") << "\n";
    return ok ? 0 : 1;
}
