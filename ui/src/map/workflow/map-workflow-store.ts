import { writable } from "svelte/store";
import type { Writable } from "svelte/store";
import type { SocketState } from "../../stores/socket";

export type MapWorkflowState =
  | "idle"
  | "loading"
  | "saving"
  | "renaming"
  | "deleting"
  | "creating"
  | "intercepting";

export interface MapWorkflow {
  state: MapWorkflowState;
  pendingName: string;
  renameMode: boolean;
  lastBackendMapId: string;
  lastBackendMapName: string;
  lastBackendRotation: number;
  error: string | null;
  pendingLoadId: string;
}

export interface MapWorkflowStore extends Writable<MapWorkflow> {
  reset: () => void;
  startNewMap: (defaultName: string) => void;
  onNewMapReceived: (defaultName: string) => void;
  startInterceptedMapRename: (defaultName: string, mapId?: string, name?: string, rotation?: number) => void;
  startLoadMap: (id: string) => void;
  finishLoadMap: (mapId: string, name: string, rotation: number) => void;
  startSaveMap: (name: string, rotation: number) => void;
  finishSaveMap: (mapId: string, name: string, rotation: number) => void;
  resetDirtyState: () => void;
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

const initial: MapWorkflow = {
  state: "idle",
  pendingName: "",
  renameMode: false,
  lastBackendMapId: "",
  lastBackendMapName: "",
  lastBackendRotation: 0,
  error: null,
  pendingLoadId: "",
};

const { subscribe, set, update } = writable<MapWorkflow>(initial);

export const mapWorkflowStore: MapWorkflowStore = {
  subscribe,
  set,
  update,
  // Placeholder actions will be overwritten by map-workflow.ts
  reset: () => set(initial),
  startNewMap: () => {},
  onNewMapReceived: () => {},
  startInterceptedMapRename: () => {},
  startLoadMap: () => {},
  finishLoadMap: () => {},
  startSaveMap: () => {},
  finishSaveMap: () => {},
  resetDirtyState: () => {},
  startRename: () => {},
  confirmRename: () => ({ action: "none", name: "" }),
  finishRename: () => {},
  cancelRename: () => {},
  startDeleteMap: () => {},
  finishDelete: () => {},
  setError: () => {},
  syncBackendState: () => {},
  setSocketUpdate: () => {},
  setSocketStateProvider: () => {},
};
