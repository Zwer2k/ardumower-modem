import { writable } from "svelte/store";
import { browser } from "$app/environment";
import type {
  ModemLog,
  ModemLogSettings,
  DesiredState,
  LogLine,
  RequestSocketMessage,
  State,
  Stats,
  ValueDescriptions,
  ConsoleLine,
  ConsoleResponseData,
  MapRaw,
  MapMeta,
  MapListData,
  SensorSummary,
  GpsDetails,
  UbxResponse,
  MowSettings,
  MowSettingsData,
} from "../model";
import { ResponseDataType, RequestDataType } from "../model";
import type { DrivenTrackData } from "../model";
import { handleMapChunk, waypointsStore, resetMapChunkBuffer } from "../map/map-chunk-buffer";
import { mowSettingsStore } from "../map/mow-settings";
import { drivenTrackStore } from "../map/service";

export interface SocketState {
  socket: WebSocket | null;
  connected: boolean;
  valueDescriptions: ValueDescriptions | null;
  state: State | null;
  stats: Stats | null;
  desiredState: DesiredState | null;
  sensorSummary: SensorSummary | null;
  gpsDetails: GpsDetails | null;
  ubxResponse: UbxResponse | null;
  modemLog: LogLine[];
  consoleLines: ConsoleLine[];
  modemDbgLevel: number;
  mapRaw: MapRaw | null;
  mapEnabled: boolean;
  liveMapEnabled: boolean;
  gpsDashboardEnabled: boolean;
  maps: MapMeta[];
  activeMapId: string;
  currentMapMeta: { hash: string; crc: number; area: number; rotation: number } | null;
}

const initialState: SocketState = {
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
  mapEnabled: false,
  liveMapEnabled: false,
  gpsDashboardEnabled: false,
  maps: [],
  activeMapId: "",
  currentMapMeta: null,
};

export const socketStore = writable<SocketState>(initialState);

/** Dedizierter Store für MotorPlot-Daten. Wird im WS-Handler befüllt und
 *  ist NICHT von clearConsoleLines() betroffen (vermeidet Race Condition
 *  mit dem Terminal, das sofort nach Empfang der Lines cleart). */
export const motorPlotStore = writable<ConsoleLine[]>([]);
export const mapMetaStore = writable<{ hash: string; crc: number; area: number; rotation: number } | null>(null);

export function clearMotorPlotStore() {
  motorPlotStore.set([]);
}

class SocketService {
  private restartTimer: NodeJS.Timeout | null = null;
  private reconnect = true;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 10;
  private connectionTimeout: NodeJS.Timeout | null = null;
  private heartbeatInterval: NodeJS.Timeout | null = null;
  private livenessInterval: NodeJS.Timeout | null = null;
  private lastMessageTime: number = 0;
  private isPageVisible = true;
  private pendingMessages: RequestSocketMessage[] = [];

  constructor() {
    if (browser) {
      document.addEventListener(
        "visibilitychange",
        this.handleVisibilityChange.bind(this),
      );
    }
  }

  connect() {
    if (!browser) {
      return;
    }

    socketStore.update((state) => {
      if (
        state.socket != null &&
        (state.socket.readyState === WebSocket.CONNECTING ||
         state.socket.readyState === WebSocket.CLOSING)
      ) {
        return state;
      }

      if (state.socket != null && state.socket.readyState === WebSocket.OPEN) {
        return state;
      }

      if (!this.reconnect || !this.isPageVisible) {
        return state;
      }

      this.clearAllTimers();
      this.pendingMessages = [];

      if (state.socket != null) {
        state.socket.close();
      }

      let host = location.host;

      try {
        const socket = new WebSocket("ws://" + host + "/ws");

        this.connectionTimeout = setTimeout(() => {
          if (socket && socket.readyState === WebSocket.CONNECTING) {
            socket.close();
          }
        }, 5000);

        socket.addEventListener("open", () => {
          // Ignoriere Events von veralteten Sockets
          let isCurrent = false;
          socketStore.update((s) => {
            isCurrent = s.socket === socket;
            return s;
          });
          if (!isCurrent) {
            try { socket.close(); } catch (_) {}
            return;
          }

          this.reconnectAttempts = 0;

          if (this.connectionTimeout) {
            clearTimeout(this.connectionTimeout);
            this.connectionTimeout = null;
          }

          this.startHeartbeat(socket);
          this.startLivenessCheck(socket);

          socketStore.update((s) => ({ ...s, connected: true }));

          // Pending-Nachrichten senden, die während der Verbindungslosigkeit aufgelaufen sind
          const pending = this.pendingMessages;
          this.pendingMessages = [];
          for (const msg of pending) {
            if (socket.readyState === WebSocket.OPEN) {
              socket.send(JSON.stringify(msg));
            }
          }

          // Aktuelle MowSettings vom Backend abrufen
          this.requestMowSettings();
        });

        socket.addEventListener("close", (event) => {
          this.clearAllTimers();

          socketStore.update((s) => {
            // Nur überschreiben, wenn dieser Socket noch der aktuelle ist
            if (s.socket === socket) {
              return { ...s, socket: null, connected: false };
            }
            return s;
          });

          if (
            this.reconnect &&
            this.reconnectAttempts < this.maxReconnectAttempts &&
            this.isPageVisible
          ) {
            this.reconnectAttempts++;
            const delay = Math.min(
              1000 * Math.pow(2, this.reconnectAttempts - 1),
              30000,
            );

            this.restartTimer = setTimeout(() => {
              this.connect();
            }, delay);
          } else if (this.reconnectAttempts >= this.maxReconnectAttempts) {
          }
        });

        socket.addEventListener("error", (error) => {
          this.clearAllTimers();
          if (this.connectionTimeout) {
            clearTimeout(this.connectionTimeout);
            this.connectionTimeout = null;
          }
          socketStore.update((s) => {
            if (s.socket === socket) {
              return { ...s, socket: null, connected: false };
            }
            return s;
          });
          if (socket) {
            try { socket.close(); } catch (_) {}
          }
        });

        socket.addEventListener("message", (message: any) => {
          this.lastMessageTime = Date.now();
          try {
            let jsonData = JSON.parse(message.data);
            socketStore.update((state) => {
              const newState = { ...state };
              switch (jsonData.type) {
                case ResponseDataType.hello:
                  newState.valueDescriptions =
                    jsonData.data as ValueDescriptions;
                  newState.modemDbgLevel = newState.valueDescriptions.logLevel;
                  newState.mapEnabled = !!(jsonData.data as any).mapEnabled;
                  newState.liveMapEnabled = !!(jsonData.data as any).liveMapEnabled;
                  newState.gpsDashboardEnabled = !!(jsonData.data as any).gpsDashboardEnabled;
                  break;
                case ResponseDataType.mowerState: {
                  const st = (jsonData.data as State) || null;
                  if (st) {
                    if (jsonData.progressPct !== undefined) st.progressPct = jsonData.progressPct;
                    if (jsonData.progressMsg !== undefined) st.progressMsg = jsonData.progressMsg;
                    // progressOp is sent as a top-level field by the backend but is
                    // not part of the `State` TS type. Preserve it on the object
                    // for the UI by assigning to `any` so Map.svelte can read it.
                    if (jsonData.progressOp !== undefined) (st as any).progressOp = jsonData.progressOp;
                  }
                  newState.state = st;
                }
                  break;
                case ResponseDataType.mowerStats:
                  newState.stats = jsonData.data as Stats;
                  break;
                case ResponseDataType.desiredState:
                  newState.desiredState = jsonData.data as DesiredState;
                  break;
                case ResponseDataType.modemLog:
                  newState.modemLog = [
                    ...newState.modemLog,
                    ...(jsonData.data as ModemLog).log,
                  ];
                  if (newState.modemLog.length > 1000) {
                    newState.modemLog = newState.modemLog.slice(-1000);
                  }
                  break;
                case ResponseDataType.mowerConsole:
                  const incomingLines = (jsonData.data as ConsoleResponseData)
                    .lines;
                  newState.consoleLines = [
                    ...newState.consoleLines,
                    ...incomingLines,
                  ];
                  if (newState.consoleLines.length > 1000) {
                    newState.consoleLines = newState.consoleLines.slice(-1000);
                  }
                  // MotorPlot-Daten separat sammeln (nicht von Terminal clearen)
                  motorPlotStore.update((existing) => [
                    ...existing,
                    ...incomingLines,
                  ]);
                  break;
                case ResponseDataType.map: {
                  const data = jsonData.data as any;
                  if (data && data.meta) {
                    const meta = { hash: data.meta.hash, crc: data.meta.crc || 0, area: data.meta.area, rotation: data.meta.rotation || 0 };
                    newState.currentMapMeta = meta;
                    mapMetaStore.set(meta);
                  }
                  // Map-Chunk-Logik: Chunks sammeln, MapStore wird im Buffer gesetzt
                  if (
                    data &&
                    data.startIndex !== undefined &&
                    data.points
                  ) {
                    handleMapChunk(data);
                  }
                  break;
                }
                case ResponseDataType.mapList: {
                  const listData = jsonData.data as MapListData;
                  newState.maps = listData.maps || [];
                  newState.activeMapId = listData.activeId || "";
                  break;
                }
                case ResponseDataType.sensorSummary:
                  newState.sensorSummary = jsonData.data as SensorSummary;
                  break;
                case ResponseDataType.gpsDetails:
                  newState.gpsDetails = jsonData.data as GpsDetails;
                  break;
                case ResponseDataType.ubxResponse:
                  newState.ubxResponse = jsonData.data as UbxResponse;
                  break;
                case ResponseDataType.mowSettings:
                  mowSettingsStore.set(jsonData.data as MowSettings);
                  break;
                case ResponseDataType.drivenTrack:
                  drivenTrackStore.set(jsonData.data as DrivenTrackData);
                  break;
                default:
              }
              return newState;
            });
          } catch (error) {
            console.error(
              "[Socket] Error parsing message:",
              error,
              message.data,
            );
          }
        });

        return { ...state, socket };
      } catch (error) {
        this.clearAllTimers();
        this.reconnectAttempts++;
        if (
          this.reconnect &&
          this.reconnectAttempts < this.maxReconnectAttempts &&
          this.isPageVisible
        ) {
          const delay = Math.min(
            1000 * Math.pow(2, this.reconnectAttempts - 1),
            30000,
          );
          this.restartTimer = setTimeout(() => {
            this.connect();
          }, delay);
        }
        return state;
      }
    });
  }

  sendMessage(message: RequestSocketMessage) {
    socketStore.update((state) => {
      if (
        browser &&
        state.socket &&
        state.socket.readyState === WebSocket.OPEN
      ) {
        state.socket.send(JSON.stringify(message));
      } else if (browser) {
        this.pendingMessages.push(message);
      }
      return state;
    });
  }

  setLogLevel(logLevel: number) {
    socketStore.update((state) => {
      const newState = { ...state, modemDbgLevel: logLevel };

      if (
        browser &&
        state.socket &&
        state.socket.readyState === WebSocket.OPEN
      ) {
        const settings: RequestSocketMessage = {
          type: RequestDataType.modemLogSettings,
          data: { logLevel } as ModemLogSettings,
        };
        state.socket.send(JSON.stringify(settings));
      }

      return newState;
    });
  }

  sendConsoleCommand(cmd: string) {
    const req: RequestSocketMessage = {
      type: RequestDataType.mowerConsoleRequest,
      data: { cmd } as ConsoleRequestData,
    };
    this.sendMessage(req);
  }

  clearConsoleLines() {
    socketStore.update((state) => ({ ...state, consoleLines: [] }));
  }

  requestGpsDetails() {
    const req: RequestSocketMessage = {
      type: RequestDataType.requestGpsDetails,
      data: {},
    };
    this.sendMessage(req);
  }

  stopGpsDetails() {
    const req: RequestSocketMessage = {
      type: RequestDataType.stopGpsDetails,
      data: {},
    };
    this.sendMessage(req);
  }

  requestSensorSummary() {
    const req: RequestSocketMessage = {
      type: RequestDataType.requestSensorSummary,
      data: {},
    };
    this.sendMessage(req);
  }

  stopSensorSummary() {
    const req: RequestSocketMessage = {
      type: RequestDataType.stopSensorSummary,
      data: {},
    };
    this.sendMessage(req);
  }

  sendUbx(hex: string) {
    const req: RequestSocketMessage = {
      type: RequestDataType.requestUbx,
      data: { hex },
    };
    this.sendMessage(req);
  }

  sendJoystickMove(linear: number, angular: number) {
    const req: RequestSocketMessage = {
      type: RequestDataType.joystickMove,
      data: { linear, angular },
    };
    this.sendMessage(req);
  }

  sendNavigateTo(x: number, y: number) {
    const req: RequestSocketMessage = {
      type: RequestDataType.navigateTo,
      data: { x, y },
    };
    this.sendMessage(req);
  }

  sendMap(mapData: import("../model").MapSetData) {
    const req: RequestSocketMessage = {
      type: RequestDataType.setMap,
      data: mapData,
    };
    this.sendMessage(req);
  }

  sendUploadMap() {
    const req: RequestSocketMessage = {
      type: RequestDataType.uploadMap,
      data: {},
    };
    this.sendMessage(req);
  }

  sendMowSettings(data: MowSettingsData) {
    const req: RequestSocketMessage = {
      type: RequestDataType.setMowSettings,
      data,
    };
    this.sendMessage(req);
  }

  requestMowSettings() {
    const req: RequestSocketMessage = {
      type: RequestDataType.requestMowSettings,
      data: {},
    };
    this.sendMessage(req);
  }

  sendClearWaypoints() {
    const req: RequestSocketMessage = {
      type: RequestDataType.clearWaypoints,
      data: {},
    };
    this.sendMessage(req);
    waypointsStore.set([]);
  }

  sendCalculateWaypoints() {
    const req: RequestSocketMessage = {
      type: RequestDataType.calculateWaypoints,
      data: {},
    };
    this.sendMessage(req);
    resetMapChunkBuffer();
  }

  sendListMaps() {
    const req: RequestSocketMessage = {
      type: RequestDataType.listMaps,
      data: {},
    };
    this.sendMessage(req);
  }

  sendLoadMap(id: string) {
    const req: RequestSocketMessage = {
      type: RequestDataType.loadMap,
      data: { id },
    };
    this.sendMessage(req);
  }

  sendSaveMap(name: string, rotation: number = 0) {
    const req: RequestSocketMessage = {
      type: RequestDataType.saveMap,
      data: { name, rotation },
    };
    this.sendMessage(req);
  }

  sendRenameMap(id: string, name: string) {
    const req: RequestSocketMessage = {
      type: RequestDataType.renameMap,
      data: { id, name },
    };
    this.sendMessage(req);
  }

  sendDeleteMap(id: string) {
    const req: RequestSocketMessage = {
      type: RequestDataType.deleteMap,
      data: { id },
    };
    this.sendMessage(req);
  }

  disconnect() {
    this.reconnect = false;
    this.clearAllTimers();
    this.pendingMessages = [];

    socketStore.update((state) => {
      if (state.socket) {
        if (
          state.socket.readyState === WebSocket.OPEN ||
          state.socket.readyState === WebSocket.CONNECTING
        ) {
          state.socket.close();
        }
      }
      return { ...state, socket: null, connected: false };
    });
  }

  private clearAllTimers() {
    if (this.restartTimer != null) {
      clearTimeout(this.restartTimer);
      this.restartTimer = null;
    }
    if (this.connectionTimeout != null) {
      clearTimeout(this.connectionTimeout);
      this.connectionTimeout = null;
    }
    if (this.heartbeatInterval != null) {
      clearInterval(this.heartbeatInterval);
      this.heartbeatInterval = null;
    }
    if (this.livenessInterval != null) {
      clearInterval(this.livenessInterval);
      this.livenessInterval = null;
    }
  }

  private startLivenessCheck(socket: WebSocket) {
    this.lastMessageTime = Date.now();
    if (this.livenessInterval) {
      clearInterval(this.livenessInterval);
      this.livenessInterval = null;
    }
    this.livenessInterval = setInterval(() => {
      if (socket.readyState !== WebSocket.OPEN) {
        if (this.livenessInterval) {
          clearInterval(this.livenessInterval);
          this.livenessInterval = null;
        }
        return;
      }
      if (Date.now() - this.lastMessageTime > 45000) {
        // Keine Nachricht in 45 Sekunden - Verbindung wahrscheinlich tot (z.B. ESP-Neustart)
        try { socket.close(1000, "Liveness timeout"); } catch (_) {}
      }
    }, 15000);
  }

  private startHeartbeat(socket: WebSocket) {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval);
    }

    this.heartbeatInterval = setInterval(() => {
      if (socket && socket.readyState === WebSocket.OPEN) {
        try {
          socket.send(JSON.stringify({ type: "ping" }));
        } catch (error) {
          if (socket) {
            socket.close();
          }
        }
      }
    }, 30000);
  }

  private handleVisibilityChange() {
    if (!browser) return;

    this.isPageVisible = !document.hidden;

    if (this.isPageVisible) {
      socketStore.update((state) => {
        if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
          this.reconnectAttempts = 0;
          this.connect();
        }
        return state;
      });
    } else {
      this.clearAllTimers();
    }
  }

  destroy() {
    if (!browser) return;

    document.removeEventListener(
      "visibilitychange",
      this.handleVisibilityChange.bind(this),
    );
    this.disconnect();
  }
}

export const socketService = new SocketService();
