#pragma once
#include "clipper2/clipper.core.h"
#include "clipper2/clipper.engine.h"
#include "clipper2/clipper.offset.h"
#include <vector>

namespace ArduMower {
    namespace Modem {
        namespace PathPlanner {

using Point = ArduMower::Domain::Robot::MapPoint;
using Polygon = std::vector<Point>;

using namespace Clipper2Lib;

static const double CLIPPER_SCALE = 1000.0;

inline Point64 toI64(const Point &p) {
    return Point64((int64_t)(p.X * CLIPPER_SCALE), (int64_t)(p.Y * CLIPPER_SCALE));
}

inline Point toDbl(const Point64 &p) {
    return Point{(double)p.x / CLIPPER_SCALE, (double)p.y / CLIPPER_SCALE};
}

inline Path64 toI64(const Polygon &poly) {
    Path64 path;
    path.reserve(poly.size());
    for (const auto &p : poly) path.push_back(toI64(p));
    return path;
}

inline Polygon toDbl(const Path64 &path) {
    Polygon poly;
    poly.reserve(path.size());
    for (const auto &p : path) poly.push_back(toDbl(p));
    return poly;
}

inline Paths64 toI64(const std::vector<Polygon> &polys) {
    Paths64 paths;
    paths.reserve(polys.size());
    for (const auto &p : polys) paths.push_back(toI64(p));
    return paths;
}

inline std::vector<Polygon> toDbl(const Paths64 &paths) {
    std::vector<Polygon> polys;
    polys.reserve(paths.size());
    for (const auto &p : paths) polys.push_back(toDbl(p));
    return polys;
}

inline std::vector<Polygon> clipOffset(const Polygon &poly, double delta) {
    Path64 path = toI64(poly);
    ClipperOffset co(2.0, 0.25);
    co.AddPath(path, JoinType::Miter, EndType::Polygon);
    Paths64 solution;
    co.Execute(delta * CLIPPER_SCALE, solution);
    return toDbl(solution);
}

inline Paths64 doClipOp(ClipType ct, const Paths64 &subj, const Paths64 &clip) {
    Clipper64 c;
    c.AddSubject(subj);
    c.AddClip(clip);
    Paths64 result;
    c.Execute(ct, FillRule::NonZero, result);
    return result;
}

inline std::vector<Polygon> clipDifference(const std::vector<Polygon> &subjects,
    const std::vector<Polygon> &clips) {
    return toDbl(doClipOp(ClipType::Difference, toI64(subjects), toI64(clips)));
}

inline std::vector<Polygon> clipDifference(const Polygon &subject,
    const std::vector<Polygon> &clips) {
    return toDbl(doClipOp(ClipType::Difference, Paths64{toI64(subject)}, toI64(clips)));
}

inline std::vector<Polygon> clipDifference(const Polygon &subject,
    const Polygon &clip) {
    return toDbl(doClipOp(ClipType::Difference, Paths64{toI64(subject)}, Paths64{toI64(clip)}));
}

inline std::vector<Polygon> clipUnion(const std::vector<Polygon> &subjects) {
    return toDbl(doClipOp(ClipType::Union, toI64(subjects), Paths64()));
}

inline std::vector<Polygon> clipUnion(const Polygon &a, const Polygon &b) {
    return toDbl(doClipOp(ClipType::Union, Paths64{toI64(a), toI64(b)}, Paths64()));
}

inline std::vector<Polygon> clipIntersect(const std::vector<Polygon> &subjects,
    const Polygon &clip) {
    return toDbl(doClipOp(ClipType::Intersection, toI64(subjects), Paths64{toI64(clip)}));
}

        }
    }
}
