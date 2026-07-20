import type { Map, Point } from "../model";
import { emptyMap } from "./map-utils";

// CassandRA-compatible JSON format:
// {
//   "name": "...",
//   "description": "...",
//   "dateTime": "...",
//   "source": "...",
//   "perimeter": [{"x": ..., "y": ...}, ...],
//   "exclusions": [[{"x": ..., "y": ...}, ...], ...],
//   "dockPoints": [{"x": ..., "y": ...}, ...],
//   "wayPoints": [{"x": ..., "y": ...}, ...],
//   "rotation": 0
// }
//
// The UI uses Y-up (positive y is North). CassandRA uses Y-down (positive y
// is South/screen coordinates). We flip Y on import/export so that coordinates
// are visually consistent in both applications.

export interface CassandraMap {
  name?: string;
  description?: string;
  dateTime?: string;
  source?: string;
  perimeter?: Array<{ x: number; y: number }>;
  exclusions?: Array<Array<{ x: number; y: number }>>;
  dockPoints?: Array<{ x: number; y: number }>;
  wayPoints?: Array<{ x: number; y: number }>;
  rotation?: number;
}

function flip(p: { x: number; y: number }): Point {
  return { x: p.x, y: -p.y };
}

function unflip(p: Point): { x: number; y: number } {
  return { x: p.x, y: -p.y };
}

function closedRing(points: Array<{ x: number; y: number }>): Array<{ x: number; y: number }> {
  if (points.length === 0) return points;
  const first = points[0];
  const last = points[points.length - 1];
  if (first.x === last.x && first.y === last.y) return points;
  return [...points, first];
}

export function importCassandraMap(json: string): { map: Map; rotation: number } | null {
  let parsed: CassandraMap;
  try {
    parsed = JSON.parse(json) as CassandraMap;
  } catch (e) {
    console.error("Failed to parse CassandRA map JSON", e);
    return null;
  }

  const perimeter = (parsed.perimeter || []).map(flip);
  if (perimeter.length < 3) {
    console.error("CassandRA map has too few perimeter points", perimeter.length);
    return null;
  }

  const map = emptyMap();
  map.perimeter.points = closedRing(perimeter);
  map.exclusions = (parsed.exclusions || [])
    .map((ex) => closedRing(ex.map(flip)))
    .filter((ex) => ex.length >= 3)
    .map((points) => ({ points }));
  map.dockpoints.points = (parsed.dockPoints || []).map(flip);
  map.waypoints.points = (parsed.wayPoints || []).map(flip);

  return { map, rotation: parsed.rotation ?? 0 };
}

export function exportCassandraMap(map: Map, rotation: number = 0): string {
  const payload: CassandraMap = {
    dateTime: new Date().toISOString(),
    source: "ArduMower Modem",
    perimeter: closedRing(map.perimeter.points.map(unflip)),
    exclusions: map.exclusions.map((ex) => closedRing(ex.points.map(unflip))),
    dockPoints: closedRing(map.dockpoints.points.map(unflip)),
    wayPoints: closedRing(map.waypoints.points.map(unflip)),
    rotation,
  };
  return JSON.stringify(payload, null, 2);
}

export function isValidCassandraMap(json: string): boolean {
  return importCassandraMap(json) !== null;
}
