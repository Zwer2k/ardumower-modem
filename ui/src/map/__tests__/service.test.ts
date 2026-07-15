import { describe, it, expect, beforeEach } from "vitest";
import { get } from "svelte/store";
import { cloneMap, emptyMap, MapStore, buildMapFromChunkStores, currentMapRotationStore } from "../service";
import { handleMapChunk, MapPointType, resetMapChunkBuffer } from "../map-chunk-buffer";

describe("cloneMap", () => {
  it("should create a deep copy of all point arrays", () => {
    const map = {
      perimeter: { points: [{ x: 1, y: 2 }] },
      exclusions: [{ points: [{ x: 3, y: 4 }] }],
      dockpoints: { points: [{ x: 5, y: 6 }] },
      waypoints: { points: [{ x: 7, y: 8 }] },
    };
    const cloned = cloneMap(map);

    expect(cloned).toEqual(map);
    expect(cloned.perimeter.points[0]).not.toBe(map.perimeter.points[0]);
    expect(cloned.exclusions[0].points[0]).not.toBe(map.exclusions[0].points[0]);
    expect(cloned.dockpoints.points[0]).not.toBe(map.dockpoints.points[0]);
    expect(cloned.waypoints.points[0]).not.toBe(map.waypoints.points[0]);
  });
});

describe("buildMapFromChunkStores", () => {
  beforeEach(() => {
    resetMapChunkBuffer();
    currentMapRotationStore.set(0);
  });

  it("should build a map from chunk stores with y-flipped coordinates", () => {
    handleMapChunk({ pointType: MapPointType.Perimeter, total: 1, startIndex: 0, points: [{ X: 10, Y: 20 }] });
    handleMapChunk({ pointType: MapPointType.Waypoints, total: 1, startIndex: 0, points: [{ X: 30, Y: 40 }] });
    handleMapChunk({ pointType: MapPointType.Dockpoints, total: 1, startIndex: 0, points: [{ X: 50, Y: 60 }] });
    handleMapChunk({ pointType: MapPointType.Exclusion, total: 1, startIndex: 0, exclusionIdx: 0, points: [{ X: 70, Y: 80 }] });

    const map = buildMapFromChunkStores();
    expect(map.perimeter.points).toEqual([{ x: 10, y: -20 }]);
    expect(map.waypoints.points).toEqual([{ x: 30, y: -40 }]);
    expect(map.dockpoints.points).toEqual([{ x: 50, y: -60 }]);
    expect(map.exclusions).toEqual([{ points: [{ x: 70, y: -80 }] }]);
  });
});

describe("MapStore", () => {
  beforeEach(() => {
    resetMapChunkBuffer();
    currentMapRotationStore.set(0);
  });

  it("should update when chunk stores are filled", () => {
    const before = get(MapStore);
    expect(before.map.perimeter.points).toEqual([]);

    handleMapChunk({ pointType: MapPointType.Perimeter, total: 1, startIndex: 0, points: [{ X: 10, Y: 20 }] });

    const after = get(MapStore);
    expect(after.map.perimeter.points).toEqual([{ x: 10, y: -20 }]);
    expect(after.presentation).toBeDefined();
  });
});
