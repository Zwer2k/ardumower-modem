import { writable, get } from "svelte/store";
import type { Writable } from "svelte/store";
import { MapStore, emptyMap, emptyPresentation, currentMapRotationStore } from "./service";
import { mapChunkProgress } from "./map-chunk-buffer";
import type { Map } from "./model";
import type { StoredMap } from "./service";
import type { SocketState } from "../stores/socket";

let getSocketState: (() => SocketState) | null = null;
let lastChunkProgress: { received: number; total: number; label: string } | null = null;
let snapshotTimer: ReturnType<typeof setTimeout> | null = null;
let loadingTimer: ReturnType<typeof setTimeout> | null = null;
const LOADING_TIMEOUT_MS = 5000;

export type MapWorkflowState =
  | "idle"
  | "loading"
  | "saving"
  | "renaming"
  | "deleting"
  | "creating";

export interface MapWorkflow {
  state: MapWorkflowState;
  pendingName: string;
  renameMode: boolean;
  lastBackendMapId: string;
  lastBackendMapName: string;
  lastBackendRotation: number;
  backendMapSnapshot: StoredMap | null;
  snapshotReady: boolean;
  error: string | null;
  pendingLoadId: string;
}

export interface MapWorkflowStore extends Writable<MapWorkflow> {
  reset: () => void;
  startNewMap: (defaultName: string) => void;
  onNewMapReceived: (defaultName: string) => void;
  startLoadMap: (id: string) => void;
  finishLoadMap: (mapId: string, name: string, rotation: number) => void;
  startSaveMap: (name: string, rotation: number) => void;
  finishSaveMap: (mapId: string, name: string, rotation: number) => void;
  startRename: (currentName: string) => void;
  confirmRename: (currentMapId: string, currentName: string) => { action: "save" | "rename" | "none"; name: string };
  finishRename: (name: string) => void;
  cancelRename: (currentName: string) => void;
  startDeleteMap: (id: string) => void;
  finishDelete: () => void;
  setError: (error: string) => void;
  syncBackendState: (mapId: string, name: string, rotation: number) => void;
  setSocketUpdate: (updater: ((fn: (state: SocketState) => SocketState) => void) | null) => void;
  setSocketStateProvider: (provider: (() => SocketState) | null) => void;
}

function createMapWorkflowStore(): MapWorkflowStore {
  const initial: MapWorkflow = {
    state: "idle",
    pendingName: "",
    renameMode: false,
    lastBackendMapId: "",
    lastBackendMapName: "",
    lastBackendRotation: 0,
    backendMapSnapshot: null,
    snapshotReady: false,
    error: null,
    pendingLoadId: "",
  };

  const { subscribe, set, update } = writable<MapWorkflow>(initial);
  let socketUpdate: ((fn: (state: SocketState) => SocketState) => void) | null = null;

  function updateSocket(fn: (state: SocketState) => SocketState) {
    if (socketUpdate) socketUpdate(fn);
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
      // Sicherheitsnetz: Falls der Backend mapList nicht (rechtzeitig) ankommt,
      // den Ladezustand automatisch beenden, damit die Manage-Map-Buttons
      // nicht dauerhaft deaktiviert bleiben. Wenn zwischenzeitlich eine
      // currentMapId gesetzt wurde, ist die Karte trotzdem geladen worden
      // (Backend hat mapList verloren/verzögert) -> wir beenden normal.
      const socketState = getSocketState ? getSocketState() : null;
      const mapId = socketState?.currentMapId || "";
      const maps = socketState?.maps || [];
      const map = mapId ? maps.find((m) => m.id === mapId) : undefined;
      if (map) {
        update((w) => ({
          ...w,
          state: "idle",
          pendingName: "",
          renameMode: false,
          lastBackendMapId: map.id,
          lastBackendMapName: map.name,
          lastBackendRotation: map.rotation,
          backendMapSnapshot: null,
          snapshotReady: false,
          error: null,
          pendingLoadId: "",
        }));
        updateSocket((s) => ({ ...s, isLoadingMap: false }));
      } else {
        update((w) => {
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

  return {
    subscribe,
    set,
    update,

    setSocketUpdate(updater) {
      socketUpdate = updater;
    },

    setSocketStateProvider(provider: (() => SocketState) | null) {
      getSocketState = provider;
    },

    reset() {
      set(initial);
    },

    /**
     * Startet das Erstellen einer neuen Karte (Unsaved).
     * Setzt Frontend-Map zurück und schaltet in Rename-Modus.
     * Der Aufrufer muss ggf. selbst eine Netzwerk-Anfrage absenden.
     */
    startNewMap(defaultName: string) {
      MapStore.set({ map: emptyMap, presentation: emptyPresentation });
      currentMapRotationStore.set(0);
      if (snapshotTimer) {
        clearTimeout(snapshotTimer);
        snapshotTimer = null;
      }
      updateSocket((s) => ({
        ...s,
        currentMapId: "",
        currentMapMeta: null,
        isNewMap: true,
        isLoadingMap: false,
      }));
      update((w) => ({
        ...w,
        state: "creating",
        pendingName: defaultName,
        renameMode: true,
        lastBackendMapId: "",
        lastBackendMapName: "",
        lastBackendRotation: 0,
        backendMapSnapshot: null,
        snapshotReady: false,
        error: null,
        pendingLoadId: "",
      }));
    },

    /**
     * Wird beim Empfang einer neuen/abgefangenen Karte vom Backend aufgerufen,
     * wenn keine currentMapId gesetzt ist.
     */
    onNewMapReceived(defaultName: string) {
      update((w) => {
        if (w.state !== "idle" && w.state !== "creating") return w;
        return {
          ...w,
          state: "creating",
          pendingName: w.pendingName || defaultName,
          renameMode: true,
          backendMapSnapshot: null,
          error: null,
        };
      });
    },

    /**
     * Startet den Ladevorgang einer gespeicherten Karte.
     * Der Aufrufer muss anschließend socketService.sendLoadMap(id) aufrufen.
     */
    startLoadMap(id: string) {
      if (snapshotTimer) {
        clearTimeout(snapshotTimer);
        snapshotTimer = null;
      }
      const current = get({ subscribe });
      if (current.pendingLoadId === id && current.state === "loading") {
        // Doppelte Ladeanfrage für dieselbe Karte wird ignoriert (z.B. wenn
        // das Dropdown ein zweites on:select Event feuert, während die Karte
        // noch geladen wird).
        return;
      }
      updateSocket((s) => ({
        ...s,
        currentMapMeta: null,
        currentMapId: id,
        isLoadingMap: true,
        isNewMap: false,
      }));
      update((w) => ({
        ...w,
        state: "loading",
        pendingName: "",
        renameMode: false,
        backendMapSnapshot: null,
        snapshotReady: false,
        error: null,
        pendingLoadId: id,
      }));
      startLoadingTimer();
    },

    /**
     * Wird aufgerufen, wenn eine Map erfolgreich geladen wurde (meta empfangen).
     */
    finishLoadMap(mapId: string, name: string, rotation: number) {
      if (snapshotTimer) {
        clearTimeout(snapshotTimer);
        snapshotTimer = null;
      }
      clearLoadingTimer();
      update((w) => ({
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        lastBackendMapId: mapId,
        lastBackendMapName: name,
        lastBackendRotation: rotation,
        // Snapshot wird erst aufgebaut, wenn die Map-Chunks vollständig im
        // MapStore angekommen sind (siehe recalculateIsMapDirty).
        backendMapSnapshot: null,
        snapshotReady: false,
        error: null,
        pendingLoadId: "",
      }));
    },

    /**
     * Startet den Speichervorgang (neue oder bestehende Karte).
     * Für bestehende Karten wird pendingName ignoriert, falls nicht im Rename-Modus.
     * Der Aufrufer muss anschließend socketService.sendSaveMap(name, rotation) aufrufen.
     */
    startSaveMap(name: string, rotation: number) {
      if (snapshotTimer) {
        clearTimeout(snapshotTimer);
        snapshotTimer = null;
      }
      updateSocket((s) => ({
        ...s,
        currentMapMeta: null,
        isLoadingMap: false,
        isNewMap: false,
      }));
      update((w) => ({
        ...w,
        state: "saving",
        pendingName: name,
        renameMode: false,
        snapshotReady: false,
        pendingLoadId: "",
        error: null,
      }));
    },

    /**
     * Wird aufgerufen, wenn Speichern erfolgreich war (mapList mit currentId empfangen).
     */
    finishSaveMap(mapId: string, name: string, rotation: number) {
      clearLoadingTimer();
      const snapshot = get(MapStore);
      update((w) => ({
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        lastBackendMapId: mapId,
        lastBackendMapName: name,
        lastBackendRotation: rotation,
        backendMapSnapshot: snapshot,
        snapshotReady: true,
        error: null,
      }));
    },

    /**
     * Startet den Umbenennungs-Modus für eine gespeicherte Karte.
     */
    startRename(currentName: string) {
      update((w) => ({
        ...w,
        renameMode: true,
        pendingName: currentName,
        error: null,
      }));
    },

    /**
     * Bestätigt Umbenennung.
     * Gibt zurück, welche Aktion erforderlich ist (save für neue Karte, rename für bestehende).
     */
    confirmRename(currentMapId: string, currentName: string): { action: "save" | "rename" | "none"; name: string } {
      const w = get(mapWorkflowStore);
      if (w.state === "creating" || !currentMapId) {
        update((wf) => ({ ...wf, state: "saving", renameMode: false, error: null }));
        return { action: "save", name: w.pendingName || currentName };
      }
      if (w.pendingName && w.pendingName !== currentName) {
        update((wf) => ({ ...wf, state: "renaming", renameMode: false, error: null }));
        return { action: "rename", name: w.pendingName };
      }
      update((wf) => ({ ...wf, renameMode: false, error: null }));
      return { action: "none", name: currentName };
    },

    /**
     * Wird aufgerufen, wenn Umbenennung erfolgreich war.
     */
    finishRename(name: string) {
      clearLoadingTimer();
      update((w) => ({
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        lastBackendMapName: name,
        error: null,
      }));
    },

    /**
     * Bricht Rename ab und setzt pendingName zurück.
     */
    cancelRename(currentName: string) {
      update((w) => ({
        ...w,
        pendingName: currentName,
        renameMode: false,
        error: null,
      }));
    },

    /**
     * Löscht eine gespeicherte Karte.
     * Der Aufrufer muss anschließend socketService.sendDeleteMap(id) aufrufen.
     */
    startDeleteMap(id: string) {
      update((w) => ({ ...w, state: "deleting", error: null }));
    },

    /**
     * Setzt den Workflow nach Löschen oder Fehler zurück.
     */
    finishDelete() {
      if (snapshotTimer) {
        clearTimeout(snapshotTimer);
        snapshotTimer = null;
      }
      update((w) => ({
        ...w,
        state: "idle",
        pendingName: "",
        renameMode: false,
        error: null,
      }));
    },

    /**
     * Setzt eine Fehlermeldung und geht zurück in idle.
     */
    setError(error: string) {
      clearLoadingTimer();
      update((w) => ({
        ...w,
        state: "idle",
        error,
        pendingLoadId: "",
      }));
    },

    /**
     * Aktualisiert den bekannten Backend-Zustand (z.B. nach mapList).
     */
    syncBackendState(mapId: string, name: string, rotation: number) {
      const snapshot = mapId ? get(MapStore) : null;
      update((w) => ({
        ...w,
        lastBackendMapId: mapId,
        lastBackendMapName: name,
        lastBackendRotation: rotation,
        backendMapSnapshot: snapshot,
        snapshotReady: !!snapshot,
        ...(mapId ? { pendingName: "" } : {}),
      }));
    },
  };
}

export const mapWorkflowStore = createMapWorkflowStore();

/**
 * Abgeleiteter Store: ist die aktuelle Karte im Vergleich zum Backend verändert?
 * Berücksichtigt:
 * - Neue Karte mit Geometrie
 * - Geometrie-Hash (nur wenn Backend-Hash bekannt)
 * - Rotation
 * - Name (nur im Rename-Modus für gespeicherte Karten)
 */
export const isMapDirty = writable<boolean>(false);

function arraysEqual<T>(a: T[], b: T[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    if (JSON.stringify(a[i]) !== JSON.stringify(b[i])) return false;
  }
  return true;
}

function mapsEqual(a: Map, b: Map): boolean {
  return (
    arraysEqual(a.perimeter.points, b.perimeter.points) &&
    arraysEqual(a.exclusions, b.exclusions) &&
    arraysEqual(a.dockpoints.points, b.dockpoints.points) &&
    arraysEqual(a.waypoints.points, b.waypoints.points)
  );
}

/**
 * Berechnet isMapDirty neu. Wird von socket.ts und internen Store-Subscriptions
 * aufgerufen, damit map-workflow.ts socketStore nicht direkt importieren muss
 * (vermeidet zirkulären Import).
 */
export function recalculateIsMapDirty() {
  if (!getSocketState) return;
  const $socket = getSocketState();
  const $workflow = get(mapWorkflowStore);
  const $map = get(MapStore);
  const $rotation = get(currentMapRotationStore);

  const hasGeometry = ($map.map?.perimeter?.points?.length ?? 0) >= 3;
  const isNew =
    $workflow.state === "creating" || $socket.isNewMap || !$socket.currentMapId;

  // Neue Karte: sobald Geometrie vorhanden ist, ist sie dirty.
  if (isNew) {
    isMapDirty.set(hasGeometry);
    return;
  }

  const effectiveMap = $socket.maps.find((m) => m.id === $socket.currentMapId);
  if (!effectiveMap) {
    isMapDirty.set(false);
    return;
  }

  // Gespeicherte Karte: Snapshot erstellen, sobald die geladene Karte
  // vollständig im MapStore angekommen ist. Das kann entweder passieren, wenn
  // Chunks gerade fertig geworden sind, oder wenn die Chunks bereits fertig
  // waren, bevor finishLoadMap den neuen Backend-Zustand bekannt gemacht hat.
  // Der Snapshot wird nur im idle-Zustand erstellt, damit während
  // Speichern/Umbenennen/Löschen kein Zwischenstand festgehalten wird.
  const snapshot = $workflow.backendMapSnapshot;
  const chunkProgress = get(mapChunkProgress);
  lastChunkProgress = chunkProgress;
  if (
    !$workflow.snapshotReady &&
    !snapshot &&
    $workflow.state === "idle" &&
    hasGeometry &&
    chunkProgress == null &&
    $socket.currentMapId &&
    $socket.currentMapId === $workflow.lastBackendMapId
  ) {
    if (snapshotTimer) clearTimeout(snapshotTimer);
    snapshotTimer = setTimeout(() => {
      snapshotTimer = null;
      mapWorkflowStore.update((w) => ({
        ...w,
        backendMapSnapshot: get(MapStore),
        snapshotReady: true,
      }));
    }, 0);
  }

  // Solange der Snapshot noch nicht fertig ist, gilt die Karte als nicht-dirty
  // (Buttons bleiben deaktiviert), damit nicht auf unvollständigen Daten
  // gearbeitet wird.
  if (!$workflow.snapshotReady || !snapshot) {
    isMapDirty.set(false);
    return;
  }

  const geometryDirty = !mapsEqual($map.map, snapshot.map);
  const rotationDirty = $rotation !== effectiveMap.rotation;
  const nameDirty =
    $workflow.renameMode &&
    $workflow.pendingName !== "" &&
    $workflow.pendingName !== effectiveMap.name;
  const backendUnsaved = $socket.currentMapUnsaved;

  isMapDirty.set(geometryDirty || rotationDirty || nameDirty || backendUnsaved);
}

// Interne Subscriptions: wenn sich Karte oder Rotation ändert, muss der
// Dirty-Zustand neu berechnet werden.
mapWorkflowStore.subscribe(() => recalculateIsMapDirty());
MapStore.subscribe(() => recalculateIsMapDirty());
currentMapRotationStore.subscribe(() => recalculateIsMapDirty());

if (typeof window !== "undefined") {
  (window as any).debugMapWorkflow = () => ({
    workflow: get(mapWorkflowStore),
    socket: getSocketState ? getSocketState() : null,
    mapStore: get(MapStore),
    rotation: get(currentMapRotationStore),
    chunkProgress: get(mapChunkProgress),
    isMapDirty: get(isMapDirty),
  });
}
