import { writable, derived, get } from "svelte/store";
import type {
  Map,
  MapPresentation,
  Perimeter,
  Exclusion,
  Dockpoints,
  Waypoints,
  Point,
} from "./model";
import type { DrivenTrackData, MapMeta } from "../model";
import { rotatePointsAroundOrigin } from "./geometry";
import {
  perimeterStore,
  dockpointsStore,
  exclusionsStore,
  waypointsStore,
  resetMapChunkBuffer,
} from "./map-chunk-buffer";

export const drivenTrackStore = writable<DrivenTrackData | null>(null);
export const currentMapRotationStore = writable<number>(0);

export interface StoredMap {
  map: Map;
  presentation: MapPresentation;
  meta?: MapMeta | null;
  synced?: boolean;
}
// MapStore is now derived from socketStore
// Initialize with empty map so $MapStore is always defined
export const emptyMap: Map = {
  perimeter: { points: [] },
  exclusions: [],
  dockpoints: { points: [] },
  waypoints: { points: [] },
};
export const emptyPresentation: MapPresentation = {
  boundary: { a: { x: 0, y: 0 }, b: { x: 0, y: 0 } },
  center: { x: 0, y: 0 },
  rotation: 0,
  viewBox: "0 0 1 1",
};
export let MapStore = writable<StoredMap>({
  map: emptyMap,
  presentation: emptyPresentation,
});

interface PresentationTuningParameters {
  padding: number;
}

const presentationParameterTuning: PresentationTuningParameters = {
  padding: 1,
};

const deg2rad = (deg: number): number => (deg * Math.PI) / 180;

const allMapPoints = (m: Map): Point[] => {
  const pts: Point[] = [...m.perimeter.points];
  m.exclusions.forEach((e) => pts.push(...e.points));
  pts.push(...m.dockpoints.points);
  pts.push(...m.waypoints.points);
  return pts;
};

const calculatePresentation = (
  m: Map,
  rotation: number = 0,
  p: PresentationTuningParameters = presentationParameterTuning,
): MapPresentation => {
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
};

const calculateBoundary = (
  value: Point[],
): { a: Point; b: Point; center: Point } => {
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
};

const calculateRotation = (m: Map): number => {
  if (m.perimeter.points.length < 3) return 0;

  interface edge {
    edge: Point[];
    length: number;
    orientation: number;
  }

  const dist = (a: Point[]): number =>
    Math.sqrt(Math.pow(a[0].x - a[1].x, 2) + Math.pow(a[0].y - a[1].y, 2));

  const rad2deg = (rad: number): number => (rad * 180) / Math.PI;

  const angle = (a: Point[]): number =>
    (rad2deg(Math.atan2(a[0].x - a[1].x, a[0].y - a[1].y)) + 90) % 360;

  const raw: edge[] = [...m.perimeter.points, m.perimeter.points[0]]
    .map((_, i, a) => (i === 0 ? undefined : [a[i - 1], a[i]]))
    .filter((x) => x !== undefined)
    .map((edge) => ({ edge, length: dist(edge), orientation: angle(edge) }));

  const sorted = raw.sort((a, b) => b.length - a.length);

  // console.log({ sorted, })

  return sorted[0].orientation;
};

const smallest = (a: number[]): number => a.reduce((a, b) => (a < b ? a : b), Infinity);
const largest = (a: number[]): number => a.reduce((a, b) => (a > b ? a : b), -Infinity);
const middle = (a: number[]): number =>
  Math.min(a[0], a[1]) + Math.abs(a[0] - a[1]) / 2;

const p2p = (a: { X: number; Y: number }): { x: number; y: number } => ({
  x: a.X,
  y: -a.Y,
});

// Helper to convert backend MapRaw to frontend Map
function mapRawToMap(mapRaw: any): Map {
  let perimeterPoints = (mapRaw.perimeter || []).map(p2p);
  // Fallback: Wenn perimeter leer, aber waypoints vorhanden, nutze waypoints
  if (
    perimeterPoints.length === 0 &&
    Array.isArray(mapRaw.waypoints) &&
    mapRaw.waypoints.length > 0
  ) {
    perimeterPoints = mapRaw.waypoints.map(p2p);
    console.log("[MapService] Fallback: using waypoints as perimeter");
  }
  return {
    perimeter: { points: perimeterPoints },
    exclusions: (mapRaw.exclusions || []).map((e: any) => ({
      points: e.map(p2p),
    })),
    dockpoints: { points: (mapRaw.dockpoints || []).map(p2p) },
    waypoints: { points: (mapRaw.waypoints || []).map(p2p) },
  };
}

// Map aus Chunks zusammensetzen (alle Typen)
function updateMapStore() {
  console.log("[MapService] updateMapStore called");

  let map: Map = {
    perimeter: { points: [] },
    exclusions: [],
    dockpoints: { points: [] },
    waypoints: { points: [] },
  };
  let lastSerialized: string | null = null;

  function setMap() {
    const rotation = get(currentMapRotationStore);
    const presentation = calculatePresentation(map, rotation);
    const serialized = JSON.stringify({ map, presentation });
    if (serialized === lastSerialized) return;
    lastSerialized = serialized;
    MapStore.set({ map, presentation });
    console.log("[MapService] Map updated:", { map, presentation });
  }

  perimeterStore.subscribe((arr) => {
    map.perimeter = { points: arr.map(({ X, Y }) => ({ x: X, y: -Y })) };
    setMap();
  });
  dockpointsStore.subscribe((arr) => {
    map.dockpoints = { points: arr.map(({ X, Y }) => ({ x: X, y: -Y })) };
    setMap();
  });
  exclusionsStore.subscribe((arrs) => {
    map.exclusions = arrs.map((arr) => ({
      points: arr.map(({ X, Y }) => ({ x: X, y: -Y })),
    }));
    setMap();
  });
  waypointsStore.subscribe((arr) => {
    map.waypoints = { points: arr.map(({ X, Y }) => ({ x: X, y: -Y })) };
    setMap();
  });
  currentMapRotationStore.subscribe(() => {
    setMap();
  });
}

if (!import.meta.hot || !import.meta.hot.data.updateMapStoreCalled) {
  updateMapStore();
  if (import.meta.hot) {
    import.meta.hot.data.updateMapStoreCalled = true;
  }
}

