// Buffer-Logik für Map-Chunks (Waypoints) aus WebSocket
import { writable } from 'svelte/store';
import type { MapPointRaw } from '../model';

export interface MapChunk {
  startIndex: number;
  total: number;
  waypoints: MapPointRaw[];
}

// Interner Buffer für die aktuelle Map
let buffer: MapPointRaw[] = [];
let expectedTotal = 0;

export const waypointsStore = writable<MapPointRaw[]>([]);

export function handleMapChunk(chunk: MapChunk) {
  console.log('[MapChunkBuffer] handleMapChunk received:', chunk);
  // Buffer ggf. initialisieren
  if (chunk.startIndex === 0 || chunk.total !== expectedTotal) {
    console.log('[MapChunkBuffer] Initialisiere Buffer: total', chunk.total, 'alt:', expectedTotal);
    buffer = new Array(chunk.total);
    expectedTotal = chunk.total;
  }
  // Block einfügen
  for (let i = 0; i < chunk.waypoints.length; i++) {
    console.log('[MapChunkBuffer] Setze Waypoint an', chunk.startIndex + i, chunk.waypoints[i]);
    buffer[chunk.startIndex + i] = chunk.waypoints[i];
  }
  const filled = buffer.filter(Boolean).length;
  console.log('[MapChunkBuffer] Buffer-Status:', filled, '/', expectedTotal, buffer);
  // Prüfen ob Map komplett
  if (filled === expectedTotal) {
    console.log('[MapChunkBuffer] Map komplett, setze waypointsStore');
    waypointsStore.set([...buffer]);
  }
}

export function resetMapChunkBuffer() {
  buffer = [];
  expectedTotal = 0;
  waypointsStore.set([]);
}
