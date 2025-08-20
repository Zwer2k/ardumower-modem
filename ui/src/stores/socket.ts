import { writable } from 'svelte/store';
import { browser } from '$app/environment';
import type { 
    ModemLog, 
    ModemLogSettings, 
    DesiredState, 
    LogLine, 
    RequestSocketMessage, 
    State, 
    ValueDescriptions, 
    ConsoleLine, 
    ConsoleResponseData, 
    ConsoleRequestData, 
    MapRaw 
} from "../model";
import { ResponseDataType, RequestDataType } from "../model";
import { handleMapChunk } from '../map/map-chunk-buffer';


export interface SocketState {
    socket: WebSocket | null;
    connected: boolean;
    valueDescriptions: ValueDescriptions | null;
    state: State | null;
    desiredState: DesiredState | null;
    modemLog: LogLine[];
    consoleLines: ConsoleLine[];
    modemDbgLevel: number;
    mapRaw: MapRaw | null;
}

const initialState: SocketState = {
    socket: null,
    connected: false,
    valueDescriptions: null,
    state: null,
    desiredState: null,
    modemLog: [],
    consoleLines: [],
    modemDbgLevel: 15,
    mapRaw: null
};

export const socketStore = writable<SocketState>(initialState);

class SocketService {
    private restartTimer: NodeJS.Timeout | null = null;
    private reconnect = true;
    private reconnectAttempts = 0;
    private maxReconnectAttempts = 10;
    private connectionTimeout: NodeJS.Timeout | null = null;
    private heartbeatInterval: NodeJS.Timeout | null = null;
    private isPageVisible = true;

    constructor() {
        if (browser) {
            document.addEventListener('visibilitychange', this.handleVisibilityChange.bind(this));
        }
    }

    connect() {
        if (!browser) {
            return;
        }

        socketStore.update(state => {
            if (state.socket != null && state.socket.readyState === WebSocket.CONNECTING) {
                return state;
            }
            
            if (state.socket != null && state.socket.readyState === WebSocket.OPEN) {
                return state;
            }

            if (!this.reconnect || !this.isPageVisible) {
                return state;
            }
            
            this.clearAllTimers();

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
                    this.reconnectAttempts = 0;
                    
                    if (this.connectionTimeout) {
                        clearTimeout(this.connectionTimeout);
                        this.connectionTimeout = null;
                    }
                    
                    this.startHeartbeat(socket);
                    
                    socketStore.update(s => ({ ...s, connected: true }));
                });

                socket.addEventListener('close', (event) => {
                    
                    this.clearAllTimers();
                    
                    socketStore.update(s => ({ ...s, socket: null, connected: false }));
                    
                    if (this.reconnect && this.reconnectAttempts < this.maxReconnectAttempts && this.isPageVisible) {
                        this.reconnectAttempts++;
                        const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts - 1), 30000);
                        
                        this.restartTimer = setTimeout(() => {
                            this.connect();
                        }, delay);
                    } else if (this.reconnectAttempts >= this.maxReconnectAttempts) {
                    }
                });

                socket.addEventListener("error", (error) => {
                    this.clearAllTimers();
                    if (socket) {
                        socket.close();
                    }
                });

                socket.addEventListener("message", (message: any) => {
                    try {
                        let jsonData = JSON.parse(message.data);
                        socketStore.update(state => {
                            const newState = { ...state };
                            switch (jsonData.type) {
                                case ResponseDataType.hello:
                                    newState.valueDescriptions = jsonData.data as ValueDescriptions;
                                    newState.modemDbgLevel = newState.valueDescriptions.logLevel;
                                    break;
                                case ResponseDataType.mowerState:
                                    newState.state = jsonData.data as State;
                                    break;
                                case ResponseDataType.desiredState:
                                    newState.desiredState = jsonData.data as DesiredState;
                                    break;
                                case ResponseDataType.modemLog:
                                    newState.modemLog = (jsonData.data as ModemLog).log;
                                    break;
                                case ResponseDataType.mowerConsole:
                                    newState.consoleLines = (jsonData.data as ConsoleResponseData).lines;
                                    break;
                                case ResponseDataType.map:
                                    // Map-Chunk-Logik: Chunks sammeln, MapStore wird im Buffer gesetzt
                                    if (jsonData.data && jsonData.data.startIndex !== undefined && jsonData.data.points) {
                                        handleMapChunk(jsonData.data);
                                    }
                                    break;
                                default:
                            }
                            return newState;
                        });
                    } catch (error) {
                        console.error('[Socket] Error parsing message:', error, message.data);
                    }
                });

                return { ...state, socket };
            } catch (error) {
                this.clearAllTimers();
                this.reconnectAttempts++;
                if (this.reconnect && this.reconnectAttempts < this.maxReconnectAttempts && this.isPageVisible) {
                    const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts - 1), 30000);
                    this.restartTimer = setTimeout(() => {
                        this.connect();
                    }, delay);
                }
                return state;
            }
        });
    }

    sendMessage(message: RequestSocketMessage) {
        socketStore.update(state => {
            if (browser && state.socket && state.socket.readyState === WebSocket.OPEN) {
                state.socket.send(JSON.stringify(message));
            }
            return state;
        });
    }

    setLogLevel(logLevel: number) {
        socketStore.update(state => {
            const newState = { ...state, modemDbgLevel: logLevel };
            
            if (browser && state.socket && state.socket.readyState === WebSocket.OPEN) {
                const settings: RequestSocketMessage = {
                    type: RequestDataType.modemLogSettings,
                    data: { logLevel } as ModemLogSettings
                };
                state.socket.send(JSON.stringify(settings));
            }
            
            return newState;
        });
    }

    sendConsoleCommand(cmd: string) {
        const req: RequestSocketMessage = {
            type: RequestDataType.mowerConsoleRequest,
            data: { cmd } as ConsoleRequestData
        };
        this.sendMessage(req);
    }

    clearConsoleLines() {
        socketStore.update(state => ({ ...state, consoleLines: [] }));
    }

    disconnect() {
        this.reconnect = false;
        this.clearAllTimers();
        
        socketStore.update(state => {
            if (state.socket) {
                if (state.socket.readyState === WebSocket.OPEN || state.socket.readyState === WebSocket.CONNECTING) {
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
    }

    private startHeartbeat(socket: WebSocket) {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
        }
        
        this.heartbeatInterval = setInterval(() => {
            if (socket && socket.readyState === WebSocket.OPEN) {
                try {
                    socket.send(JSON.stringify({ type: 'ping' }));
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
            socketStore.update(state => {
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
        
        document.removeEventListener('visibilitychange', this.handleVisibilityChange.bind(this));
        this.disconnect();
    }
}

export const socketService = new SocketService();
