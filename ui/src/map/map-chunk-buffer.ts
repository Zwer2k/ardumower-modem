// Buffer-Logik für Map-Chunks (Waypoints) aus WebSocket

import { writable } from 'svelte/store';
import type { MapPoint } from '../model';

// Enum-Werte müssen mit Backend übereinstimmen!
export enum MapPointType {
  Perimeter = 0,
  Exclusion = 1,
  Dockpoints = 2,
  Waypoints = 3,
}

export interface MapChunk {
  startIndex: number;
  total: number;
  pointType: MapPointType;
  exclusionIdx?: number;
  points: MapPoint[];
}

// Buffer für alle Typen
let perimeterBuffer: MapPoint[] = [];
let perimeterTotal = 0;
let dockpointsBuffer: MapPoint[] = [];
let dockpointsTotal = 0;
let waypointsBuffer: MapPoint[] = [];
let waypointsTotal = 0;
let exclusionsBuffer: MapPoint[][] = [];
let exclusionsTotal: number[] = [];

export const perimeterStore = writable<MapPoint[]>([]);
export const dockpointsStore = writable<MapPoint[]>([]);
export const waypointsStore = writable<MapPoint[]>([]);
export const exclusionsStore = writable<MapPoint[][]>([]);

// Progress during chunk reception (download from backend)
export const mapChunkProgress = writable<{ received: number; total: number; label: string } | null>(null);

export function handleMapChunk(chunk: MapChunk) {
  //console.log('[MapChunkBuffer] handleMapChunk received:', chunk);
  // Helper for shared logic, chunk is in closure
  function handleGeneric(buffer: MapPoint[], total: number, store: typeof perimeterStore): [MapPoint[], number] {
    if (chunk.startIndex === 0 || chunk.total !== total) {
      buffer = new Array(chunk.total);
      total = chunk.total;
    }
    for (let i = 0; i < chunk.points.length; i++) {
      buffer[chunk.startIndex + i] = chunk.points[i];
    }
    const filled = buffer.filter(Boolean).length;
    //console.log(`[MapChunkBuffer] ${chunk.pointType} buffer filled:`, filled, total);
    if (filled === total) {
      store.set([...buffer]);
    }
    return [buffer, total];
  }

  if (chunk.pointType === MapPointType.Exclusion) {
    const idx = chunk.exclusionIdx ?? 0;
    if (!exclusionsBuffer[idx] || chunk.startIndex === 0 || chunk.total !== exclusionsTotal[idx]) {
      exclusionsBuffer[idx] = new Array(chunk.total);
      exclusionsTotal[idx] = chunk.total;
    }
    for (let i = 0; i < chunk.points.length; i++) {
      exclusionsBuffer[idx][chunk.startIndex + i] = chunk.points[i];
    }
    const filled = exclusionsBuffer[idx].filter(Boolean).length;
    if (filled === exclusionsTotal[idx]) {
      exclusionsStore.set([...exclusionsBuffer]);
    }
    return;
  }

  //console.log('[MapChunkBuffer] Handling chunk for pointType:', chunk.pointType);
  let progressLabel = '';
  switch (chunk.pointType) {
    case MapPointType.Perimeter:
      [perimeterBuffer, perimeterTotal] = handleGeneric(perimeterBuffer, perimeterTotal, perimeterStore);
      progressLabel = 'Perimeter';
      break;
    case MapPointType.Dockpoints:
      [dockpointsBuffer, dockpointsTotal] = handleGeneric(dockpointsBuffer, dockpointsTotal, dockpointsStore);
      progressLabel = 'Dockpoints';
      break;
    case MapPointType.Waypoints:
      [waypointsBuffer, waypointsTotal] = handleGeneric(waypointsBuffer, waypointsTotal, waypointsStore);
      progressLabel = 'Waypoints';
      break;
    default:
      console.warn('[MapChunkBuffer] Unbekannter pointType:', chunk.pointType);
  }
  if (progressLabel && chunk.total > 0) {
    const received = Math.min(chunk.startIndex + chunk.points.length, chunk.total);
    mapChunkProgress.set({ received, total: chunk.total, label: progressLabel });
    if (received >= chunk.total) {
      mapChunkProgress.set(null);
    }
  }
}


export function resetMapChunkBuffer() {
  perimeterBuffer = [];
  perimeterTotal = 0;
  dockpointsBuffer = [];
  dockpointsTotal = 0;
  waypointsBuffer = [];
  waypointsTotal = 0;
  exclusionsBuffer = [];
  exclusionsTotal = [];
  perimeterStore.set([]);
  dockpointsStore.set([]);
  waypointsStore.set([]);
  exclusionsStore.set([]);
}
