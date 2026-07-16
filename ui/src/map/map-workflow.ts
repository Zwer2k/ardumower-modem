import { get } from "svelte/store";
import { MapStore, emptyMap, emptyPresentation, currentMapRotationStore } from "./service";
import { isMapDirty, setMapDirty } from "./services/map-sync";
import { mapWorkflowStore, type MapWorkflowStore, type MapWorkflowState } from "./workflow/map-workflow-store";
import type { SocketState } from "../stores/socket";
import { Error as ErrorStore } from "../stores/error";

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

function isNameUsed(name: string, excludeId?: string): boolean {
  const socketState = getSocketState ? getSocketState() : null;
  if (!socketState) return false;
  const maps = socketState.maps || [];
  return maps.some((m) => m.name === name && (!excludeId || m.id !== excludeId));
}

function generateUniqueName(baseName: string, excludeId?: string): string {
  if (!isNameUsed(baseName, excludeId)) return baseName;
  // Versuche "Karte X" aus dem Basisnamen zu extrahieren, um den Zähler
  // hochzusetzen, anstatt jedes Mal ein Suffix anzuhängen.
  const match = baseName.match(/^(.*\D)(\d+)$/);
  const prefix = match ? match[1].trimEnd() : baseName;
  let counter = match ? parseInt(match[2], 10) + 1 : 2;
  let candidate = `${prefix} ${counter}`;
  while (isNameUsed(candidate, excludeId)) {
    counter++;
    candidate = `${prefix} ${counter}`;
  }
  return candidate;
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
    const uniqueName = generateUniqueName(defaultName);
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
      pendingName: uniqueName,
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

  startInterceptedMapRename(defaultName: string, mapId?: string, name?: string, rotation?: number) {
    // Abgefangene Karte: Rename-Modus sofort starten, aber kein isNewMap.
    // Das Speichern in SPIFFS wird vermieden; stattdessen wird später
    // nur ein renameMap an das Backend gesendet.
    const uniqueName = generateUniqueName(defaultName, mapId);
    updateSocket((s) => ({
      ...s,
      isNewMap: false,
      isLoadingMap: false,
    }));
    mapWorkflowStore.update((w) => ({
      ...w,
      state: "intercepting",
      pendingName: w.pendingName || uniqueName,
      renameMode: true,
      lastBackendMapId: mapId ?? "",
      lastBackendMapName: name ?? "",
      lastBackendRotation: rotation ?? 0,
      error: null,
      pendingLoadId: "",
    }));
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
    mapWorkflowStore.update((w) => {
      // Während der Rename-Abfrage für eine abgefangene Karte darf die
      // Map-Liste den Dialog nicht beenden. Meta/Id werden übernommen, der
      // Rename-Modus bleibt aktiv.
      if (w.state === "intercepting" && w.renameMode) {
        return {
          ...w,
          lastBackendMapId: mapId,
          lastBackendMapName: name,
          lastBackendRotation: rotation,
          error: null,
        };
      }
      return {
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        lastBackendMapId: mapId,
        lastBackendMapName: name,
        lastBackendRotation: rotation,
        error: null,
        pendingLoadId: "",
      };
    });
    // Der dirty-Status wird nicht hier zurückgesetzt, sondern vom mapList-Handler
    // in socket.ts anhand des Backend-Flags currentMapUnsaved synchronisiert.
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
    const isTransientId = (id: string) => id.startsWith("__t_");
    const requestedName = w.pendingName || currentName;
    // Doppelte Namen verhindern, außer wenn der Name der aktuellen Karte
    // unverändert bleibt.
    if (requestedName !== currentName && isNameUsed(requestedName, currentMapId)) {
      const message = `Name "${requestedName}" ist bereits vergeben.`;
      mapWorkflowStore.update((wf) => ({ ...wf, error: message }));
      ErrorStore.set(new Error(message));
      return { action: "none", name: currentName };
    }
    // Abgefangene Karte: immer nur umbenennen, niemals speichern.
    if (w.state === "intercepting") {
      mapWorkflowStore.update((wf) => ({ ...wf, state: "renaming", renameMode: false, error: null }));
      isMapDirty.set(true);
      return { action: "rename", name: requestedName };
    }
    if (w.state === "creating" || (!currentMapId && !isTransientId(currentMapId))) {
      mapWorkflowStore.update((wf) => ({ ...wf, state: "saving", renameMode: false, error: null }));
      return { action: "save", name: requestedName };
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
    // Rename wird wie eine Bearbeitung behandelt: dirty bleibt erhalten,
    // weil der Backend-Flag currentMapUnsaved den Status treibt.
  },

  cancelRename(currentName: string) {
    const state = get(mapWorkflowStore).state;
    mapWorkflowStore.update((w) => ({
      ...w,
      pendingName: currentName,
      renameMode: false,
      error: null,
      // Abgefangene Karte bleibt auch nach Abbruch der Rename-Abfrage
      // im Workflow, damit sie weiterhin als unsaved behandelt wird.
      state: state === "intercepting" ? "idle" : w.state,
    }));
    // Dirty-Status wird nur für explizite, nicht-erzwungene Renames
    // zurückgesetzt. Bei abgefangenen Karten bleibt er vom Backend gesteuert.
    if (state !== "intercepting") {
      isMapDirty.set(false);
    }
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
