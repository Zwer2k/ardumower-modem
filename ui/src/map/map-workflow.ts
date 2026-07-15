import { get } from "svelte/store";
import { MapStore, emptyMap, emptyPresentation, currentMapRotationStore } from "./service";
import { isMapDirty, setMapDirty } from "./services/map-sync";
import { mapWorkflowStore, type MapWorkflowStore, type MapWorkflowState } from "./workflow/map-workflow-store";
import type { SocketState } from "../../stores/socket";

let getSocketState: (() => SocketState) | null = null;
let loadingTimer: ReturnType<typeof setTimeout> | null = null;
const LOADING_TIMEOUT_MS = 5000;

function updateSocket(fn: (state: SocketState) => SocketState) {
  const updater = get(mapWorkflowStore).setSocketUpdate as any;
  if (updater) updater(fn);
}

function clearLoadingTimer() {
  if (loadingTimer) {
    clearTimeout(loadingTimer);
    loadingTimer = null;
  }
}

function startLoadingTimer() {
  clearLoadingTimer();
  loadingTimer = setTimeout(() => {
    const socketState = getSocketState ? getSocketState() : null;
    const mapId = socketState?.currentMapId || "";
    const maps = socketState?.maps || [];
    const map = mapId ? maps.find((m) => m.id === mapId) : undefined;
    if (map) {
      mapWorkflowStore.update((w) => ({
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        lastBackendMapId: map.id,
        lastBackendMapName: map.name,
        lastBackendRotation: map.rotation,
        error: null,
        pendingLoadId: "",
      }));
      isMapDirty.set(false);
      updateSocket((s) => ({ ...s, isLoadingMap: false }));
    } else {
      mapWorkflowStore.update((w) => {
        if (w.state !== "loading") return w;
        return {
          ...w,
          state: "idle",
          pendingLoadId: "",
          error: "Karte konnte nicht geladen werden (Timeout)",
        };
      });
      updateSocket((s) => ({
        ...s,
        isLoadingMap: false,
      }));
    }
    clearLoadingTimer();
  }, LOADING_TIMEOUT_MS);
}

const workflowActions: Omit<MapWorkflowStore, "subscribe" | "set" | "update"> = {
  reset() {
    mapWorkflowStore.set({
      state: "idle",
      pendingName: "",
      renameMode: false,
      lastBackendMapId: "",
      lastBackendMapName: "",
      lastBackendRotation: 0,
      error: null,
      pendingLoadId: "",
    });
    isMapDirty.set(false);
  },

  startNewMap(defaultName: string) {
    MapStore.set({ map: emptyMap(), presentation: emptyPresentation() });
    currentMapRotationStore.set(0);
    updateSocket((s) => ({
      ...s,
      currentMapId: "",
      currentMapMeta: null,
      isNewMap: true,
      isLoadingMap: false,
    }));
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "creating",
      pendingName: defaultName,
      renameMode: true,
      lastBackendMapId: "",
      lastBackendMapName: "",
      lastBackendRotation: 0,
      error: null,
      pendingLoadId: "",
    }));
    isMapDirty.set(false);
  },

  onNewMapReceived(defaultName: string) {
    mapWorkflowStore.update((w) => {
      if (w.state !== "idle" && w.state !== "creating") return w;
      return {
        ...w,
        state: "creating",
        pendingName: w.pendingName || defaultName,
        renameMode: true,
        error: null,
      };
    });
  },

  startLoadMap(id: string) {
    const current = get(mapWorkflowStore);
    if (current.pendingLoadId === id && current.state === "loading") {
      return;
    }
    updateSocket((s) => ({
      ...s,
      currentMapMeta: null,
      currentMapId: id,
      isLoadingMap: true,
      isNewMap: false,
    }));
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "loading",
      pendingName: "",
      renameMode: false,
      error: null,
      pendingLoadId: id,
    }));
    isMapDirty.set(false);
    startLoadingTimer();
  },

  finishLoadMap(mapId: string, name: string, rotation: number) {
    clearLoadingTimer();
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      pendingName: "",
      renameMode: false,
      lastBackendMapId: mapId,
      lastBackendMapName: name,
      lastBackendRotation: rotation,
      error: null,
      pendingLoadId: "",
    }));
    isMapDirty.set(false);
  },

  startSaveMap(name: string, rotation: number) {
    updateSocket((s) => ({
      ...s,
      currentMapMeta: null,
      isLoadingMap: false,
      isNewMap: false,
    }));
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "saving",
      pendingName: name,
      renameMode: false,
      pendingLoadId: "",
      error: null,
    }));
  },

  finishSaveMap(mapId: string, name: string, rotation: number) {
    clearLoadingTimer();
    const currentState = get(mapWorkflowStore).state;
    if (currentState !== "saving" && currentState !== "renaming") {
      return;
    }
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      pendingName: "",
      renameMode: false,
      lastBackendMapId: mapId,
      lastBackendMapName: name,
      lastBackendRotation: rotation,
      error: null,
    }));
    isMapDirty.set(false);
  },

  resetDirtyState() {
    updateSocket((s) => ({ ...s, currentMapUnsaved: false, isNewMap: false }));
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      pendingName: "",
      renameMode: false,
      error: null,
    }));
    isMapDirty.set(false);
  },

  startRename(currentName: string) {
    mapWorkflowStore.update((w) => ({
      ...w,
      renameMode: true,
      pendingName: currentName,
      error: null,
    }));
  },

  confirmRename(currentMapId: string, currentName: string): { action: "save" | "rename" | "none"; name: string } {
    const w = get(mapWorkflowStore);
    if (w.state === "creating" || !currentMapId) {
      mapWorkflowStore.update((wf) => ({ ...wf, state: "saving", renameMode: false, error: null }));
      return { action: "save", name: w.pendingName || currentName };
    }
    if (w.pendingName && w.pendingName !== currentName) {
      mapWorkflowStore.update((wf) => ({ ...wf, state: "renaming", renameMode: false, error: null }));
      isMapDirty.set(true);
      return { action: "rename", name: w.pendingName };
    }
    mapWorkflowStore.update((wf) => ({ ...wf, renameMode: false, error: null }));
    return { action: "none", name: currentName };
  },

  finishRename(name: string) {
    clearLoadingTimer();
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      pendingName: "",
      renameMode: false,
      lastBackendMapName: name,
      error: null,
    }));
    isMapDirty.set(false);
  },

  cancelRename(currentName: string) {
    mapWorkflowStore.update((w) => ({
      ...w,
      pendingName: currentName,
      renameMode: false,
      error: null,
    }));
    isMapDirty.set(false);
  },

  startDeleteMap(id: string) {
    mapWorkflowStore.update((w) => ({ ...w, state: "deleting", error: null }));
  },

  finishDelete() {
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      pendingName: "",
      renameMode: false,
      error: null,
    }));
    isMapDirty.set(false);
  },

  setError(error: string) {
    clearLoadingTimer();
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "idle",
      error,
      pendingLoadId: "",
    }));
  },

  syncBackendState(mapId: string, name: string, rotation: number) {
    mapWorkflowStore.update((w) => ({
      ...w,
      lastBackendMapId: mapId,
      lastBackendMapName: name,
      lastBackendRotation: rotation,
      ...(mapId ? { pendingName: "" } : {}),
    }));
  },

  setSocketUpdate(updater) {
    // Handled by map-workflow-store.ts internal binding
  },

  setSocketStateProvider(provider: (() => SocketState) | null) {
    getSocketState = provider;
  },
};

// Attach actions to the shared store
Object.assign(mapWorkflowStore, workflowActions);

export { mapWorkflowStore };
export type { MapWorkflowStore, MapWorkflowState } from "./map-workflow-store";
export { isMapDirty, setMapDirty } from "./services/map-sync";
