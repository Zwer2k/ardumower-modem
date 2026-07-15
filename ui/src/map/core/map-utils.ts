import type {
  Point,
  Map,
  MapPresentation,
  Perimeter,
  Exclusion,
  Dockpoints,
  Waypoints,
} from "../model";

/** Erstellt eine tiefe Kopie einer Map (inkl. aller Punkt-Arrays). */
export function cloneMap(map: Map): Map {
  return {
    perimeter: { points: map.perimeter.points.map((p) => ({ ...p })) },
    exclusions: map.exclusions.map((e) => ({
      points: e.points.map((p) => ({ ...p })),
    })),
    dockpoints: { points: map.dockpoints.points.map((p) => ({ ...p })) },
    waypoints: { points: map.waypoints.points.map((p) => ({ ...p })) },
  };
}

/** Prüft, ob zwei Punkt-Arrays inhaltlich gleich sind. */
export function pointsEqual(a: Point[], b: Point[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    if (a[i].x !== b[i].x || a[i].y !== b[i].y) return false;
  }
  return true;
}

/** Prüft, ob zwei Maps geometrisch gleich sind. */
export function mapsEqual(a: Map, b: Map): boolean {
  if (!pointsEqual(a.perimeter.points, b.perimeter.points)) return false;
  if (a.exclusions.length !== b.exclusions.length) return false;
  for (let i = 0; i < a.exclusions.length; i++) {
    if (!pointsEqual(a.exclusions[i].points, b.exclusions[i].points)) return false;
  }
  if (!pointsEqual(a.dockpoints.points, b.dockpoints.points)) return false;
  if (!pointsEqual(a.waypoints.points, b.waypoints.points)) return false;
  return true;
}

export function emptyMap(): Map {
  return {
    perimeter: { points: [] } as Perimeter,
    exclusions: [] as Exclusion[],
    dockpoints: { points: [] } as Dockpoints,
    waypoints: { points: [] } as Waypoints,
  };
}

export function emptyPresentation(): MapPresentation {
  return {
    boundary: { a: { x: 0, y: 0 }, b: { x: 0, y: 0 } },
    center: { x: 0, y: 0 },
    rotation: 0,
    viewBox: "0 0 1 1",
  };
}

export function hasAnyMapPoints(map: Map): boolean {
  return (
    map.perimeter.points.length > 0 ||
    map.exclusions.length > 0 ||
    map.dockpoints.points.length > 0 ||
    map.waypoints.points.length > 0
  );
}

export function hasPerimeter(map: Map): boolean {
  return map.perimeter.points.length >= 3;
}
