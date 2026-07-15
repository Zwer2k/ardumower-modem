import { writable, get } from "svelte/store";
import type { Writable } from "svelte/store";
import type { Map, MapPresentation } from "./model";
import type { DrivenTrackData, MapMeta } from "../model";
import {
  perimeterStore,
  dockpointsStore,
  exclusionsStore,
  waypointsStore,
} from "./map-chunk-buffer";
import { calculatePresentation } from "./core/presentation";
import { cloneMap, emptyMap, emptyPresentation } from "./core/map-utils";

export { cloneMap, emptyMap, emptyPresentation };
export { calculatePresentation } from "./core/presentation";
export { rotatePointsAroundOrigin, pointsToEdges, pointsForPolygon, edgeArrowPath, polygonArrowPath } from "./core/geometry";

export const drivenTrackStore = writable<DrivenTrackData | null>(null);
export const currentMapRotationStore = writable<number>(0);

export interface StoredMap {
  map: Map;
  presentation: MapPresentation;
  meta?: MapMeta | null;
  synced?: boolean;
}

let MapStore: Writable<StoredMap> = writable<StoredMap>({
  map: emptyMap(),
  presentation: emptyPresentation(),
});

export { MapStore };

export function resetMapStore(): Writable<StoredMap> {
  MapStore = writable<StoredMap>({
    map: emptyMap(),
    presentation: emptyPresentation(),
  });
  return MapStore;
}

const p2p = (a: { X: number; Y: number }): { x: number; y: number } => ({
  x: a.X,
  y: -a.Y,
});

export function mapRawToMap(mapRaw: any): Map {
  let perimeterPoints = (mapRaw.perimeter || []).map(p2p);
  if (
    perimeterPoints.length === 0 &&
    Array.isArray(mapRaw.waypoints) &&
    mapRaw.waypoints.length > 0
  ) {
    perimeterPoints = mapRaw.waypoints.map(p2p);
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

export function buildMapFromChunkStores(): Map {
  const toPoint = ({ X, Y }: { X: number; Y: number }) => ({ x: X, y: -Y });
  return {
    perimeter: { points: get(perimeterStore).map(toPoint) },
    exclusions: get(exclusionsStore).map((arr) => ({ points: arr.map(toPoint) })),
    dockpoints: { points: get(dockpointsStore).map(toPoint) },
    waypoints: { points: get(waypointsStore).map(toPoint) },
  };
}

// Map aus Chunks zusammensetzen (alle Typen)
function updateMapStore() {
  let map: Map = emptyMap();
  let lastSerialized: string | null = null;

  function setMap() {
    const rotation = get(currentMapRotationStore);
    const presentation = calculatePresentation(map, rotation);
    const serialized = JSON.stringify({ map, presentation });
    if (serialized === lastSerialized) return;
    lastSerialized = serialized;
    MapStore.set({ map, presentation });
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

if (import.meta.hot) {
  if (!import.meta.hot.data?.updateMapStoreCalled) {
    updateMapStore();
    if (import.meta.hot.data) {
      import.meta.hot.data.updateMapStoreCalled = true;
    }
  }
} else {
  updateMapStore();
}

