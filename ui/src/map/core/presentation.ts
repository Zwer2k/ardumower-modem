import type { Map, MapPresentation, Point } from "../model";
import { rotatePointsAroundOrigin } from "./geometry";

interface PresentationTuningParameters {
  padding: number;
}

const presentationParameterTuning: PresentationTuningParameters = {
  padding: 1,
};

const deg2rad = (deg: number): number => (deg * Math.PI) / 180;

export function allMapPoints(m: Map): Point[] {
  const pts: Point[] = [...m.perimeter.points];
  m.exclusions.forEach((e) => pts.push(...e.points));
  pts.push(...m.dockpoints.points);
  pts.push(...m.waypoints.points);
  return pts;
}

function calculateBoundary(value: Point[]): { a: Point; b: Point; center: Point } {
  const allX: number[] = value.map(({ x }) => x).filter(Number.isFinite);
  const allY: number[] = value.map(({ y }) => y).filter(Number.isFinite);

  if (allX.length === 0 || allY.length === 0) {
    const origin: Point = { x: 0, y: 0 };
    return { a: origin, b: origin, center: origin };
  }

  const extremes = {
    x: [smallest(allX), largest(allX)],
    y: [smallest(allY), largest(allY)],
  };
  const center: Point = { x: middle(extremes.x), y: middle(extremes.y) };

  return {
    a: { x: extremes.x[0], y: extremes.y[0] },
    b: { x: extremes.x[1], y: extremes.y[1] },
    center,
  };
}

export function calculatePresentation(
  m: Map,
  rotation: number = 0,
  p: PresentationTuningParameters = presentationParameterTuning,
): MapPresentation {
  const pointsRotated: Point[] = rotatePointsAroundOrigin(
    allMapPoints(m),
    deg2rad(rotation),
  );
  const boundary = calculateBoundary(pointsRotated);
  const center = boundary.center;
  boundary.a.x -= p.padding;
  boundary.a.y -= p.padding;
  boundary.b.x += p.padding;
  boundary.b.y += p.padding;
  const viewBox = `${boundary.a.x} ${boundary.a.y} ${boundary.b.x - boundary.a.x} ${boundary.b.y - boundary.a.y}`;

  return { center, boundary, rotation, viewBox };
}

export function calculateRotation(m: Map): number {
  if (m.perimeter.points.length < 3) return 0;

  interface EdgeInfo {
    edge: Point[];
    length: number;
    orientation: number;
  }

  const dist = (a: Point[]): number =>
    Math.sqrt(Math.pow(a[0].x - a[1].x, 2) + Math.pow(a[0].y - a[1].y, 2));

  const rad2deg = (rad: number): number => (rad * 180) / Math.PI;

  const angle = (a: Point[]): number =>
    (rad2deg(Math.atan2(a[0].x - a[1].x, a[0].y - a[1].y)) + 90) % 360;

  const raw: EdgeInfo[] = [...m.perimeter.points, m.perimeter.points[0]]
    .map((_, i, a) => (i === 0 ? undefined : [a[i - 1], a[i]]))
    .filter((x): x is Point[] => x !== undefined)
    .map((edge) => ({ edge, length: dist(edge), orientation: angle(edge) }));

  const sorted = raw.sort((a, b) => b.length - a.length);
  return sorted[0].orientation;
}

const smallest = (a: number[]): number => a.reduce((a, b) => (a < b ? a : b), Infinity);
const largest = (a: number[]): number => a.reduce((a, b) => (a > b ? a : b), -Infinity);
const middle = (a: number[]): number =>
  Math.min(a[0], a[1]) + Math.abs(a[0] - a[1]) / 2;
