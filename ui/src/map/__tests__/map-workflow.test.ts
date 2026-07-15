import { describe, it, expect, beforeEach, vi, afterEach } from "vitest";
import { get } from "svelte/store";
import { MapStore, emptyMap, emptyPresentation, cloneMap, currentMapRotationStore } from "../service";
import { mapWorkflowStore, isMapDirty } from "../map-workflow";
import { resetMapChunkBuffer } from "../map-chunk-buffer";
import type { SocketState } from "../../stores/socket";

function createMockSocketState(overrides: Partial<SocketState> = {}): SocketState {
  return {
    socket: null,
    connected: false,
    valueDescriptions: null,
    state: null,
    stats: null,
    desiredState: null,
    sensorSummary: null,
    gpsDetails: null,
    ubxResponse: null,
    modemLog: [],
    consoleLines: [],
    modemDbgLevel: 15,
    mapRaw: null,
    mapEnabled: true,
    liveMapEnabled: false,
    gpsDashboardEnabled: false,
    maps: [],
    activeMapId: "",
    currentMapId: "map-1",
    currentMapMeta: null,
    currentMapUnsaved: false,
    isLoadingMap: false,
    isNewMap: false,
    ...overrides,
  };
}

function resetAllMapState() {
  resetMapChunkBuffer();
  MapStore.set({ map: cloneMap(emptyMap()), presentation: emptyPresentation() });
  currentMapRotationStore.set(0);
  mapWorkflowStore.reset();
}

describe("map-workflow dirty status", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    resetAllMapState();
    const socketState = createMockSocketState();
    mapWorkflowStore.setSocketStateProvider(() => socketState);
    mapWorkflowStore.setSocketUpdate((fn) => {
      // no-op for tests
    });
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("starts not dirty", () => {
    expect(get(isMapDirty)).toBe(false);
  });

  it("is not dirty after loading a map", () => {
    const socketState = createMockSocketState({ currentMapId: "map-1", maps: [{ id: "map-1", name: "Map 1", rotation: 0 }] });
    mapWorkflowStore.setSocketStateProvider(() => socketState);

    mapWorkflowStore.startLoadMap("map-1");
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    expect(get(isMapDirty)).toBe(false);
  });

  it("is dirty after moving a waypoint", () => {
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    expect(get(isMapDirty)).toBe(false);

    MapStore.update((s) => ({
      ...s,
      map: {
        ...s.map,
        waypoints: { points: [{ x: 2, y: 3 }] },
      },
    }));
    isMapDirty.set(true);

    expect(get(isMapDirty)).toBe(true);
  });

  it("finishSaveMap resets dirty state", () => {
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    isMapDirty.set(true);
    expect(get(isMapDirty)).toBe(true);

    mapWorkflowStore.startSaveMap("Map 1", 0);
    mapWorkflowStore.finishSaveMap("map-1", "Map 1", 0);

    expect(get(isMapDirty)).toBe(false);
  });

  it("does not reset dirty state when finishSaveMap is called outside saving state", () => {
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    isMapDirty.set(true);

    // simulate socket calling finishSaveMap without being in saving state
    mapWorkflowStore.finishSaveMap("map-1", "Map 1", 0);

    expect(get(isMapDirty)).toBe(true);
  });

  it("is not dirty when switching maps without changes", () => {
    const socketState = createMockSocketState({
      currentMapId: "map-1",
      maps: [
        { id: "map-1", name: "Map 1", rotation: 0 },
        { id: "map-2", name: "Map 2", rotation: 0 },
      ],
    });
    mapWorkflowStore.setSocketStateProvider(() => socketState);

    mapWorkflowStore.startLoadMap("map-1");
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    expect(get(isMapDirty)).toBe(false);
  });

  it("resets dirty when switching maps after editing", () => {
    const socketState = createMockSocketState({
      currentMapId: "map-1",
      maps: [
        { id: "map-1", name: "Map 1", rotation: 0 },
        { id: "map-2", name: "Map 2", rotation: 0 },
      ],
    });
    mapWorkflowStore.setSocketStateProvider(() => socketState);

    mapWorkflowStore.startLoadMap("map-1");
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    isMapDirty.set(true);
    mapWorkflowStore.startLoadMap("map-2");

    expect(get(isMapDirty)).toBe(false);
  });

  it("is dirty after rotation change", () => {
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);
    expect(get(isMapDirty)).toBe(false);

    currentMapRotationStore.set(45);
    isMapDirty.set(true);

    expect(get(isMapDirty)).toBe(true);
  });

  it("is dirty after renaming a map", () => {
    const socketState = createMockSocketState({
      currentMapId: "map-1",
      maps: [{ id: "map-1", name: "Map 1", rotation: 0 }],
    });
    mapWorkflowStore.setSocketStateProvider(() => socketState);
    mapWorkflowStore.finishLoadMap("map-1", "Map 1", 0);

    mapWorkflowStore.startRename("Map 1");
    mapWorkflowStore.update((w) => ({ ...w, pendingName: "Renamed Map" }));
    mapWorkflowStore.confirmRename("map-1", "Map 1");

    expect(get(isMapDirty)).toBe(true);
  });

  it("resetDirtyState resets dirty flag", () => {
    isMapDirty.set(true);
    mapWorkflowStore.resetDirtyState();
    expect(get(isMapDirty)).toBe(false);
  });

  it("finishDelete resets dirty flag", () => {
    isMapDirty.set(true);
    mapWorkflowStore.finishDelete();
    expect(get(isMapDirty)).toBe(false);
  });
});
