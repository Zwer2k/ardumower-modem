<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import type { ModemLog, ModemLogSettings, DesiredState, LogLine, RequestSocketMessage, State, ValueDescriptions, ConsoleLine, ConsoleResponseData, ConsoleRequestData } from "../../model";
    import { LogLevelDesc, RequestDataType, ResponseDataType } from "../../model";
    import Console from "./Console.svelte";
    import { Accordion, AccordionItem } from "carbon-components-svelte";
    import type { DropdownItem } from "carbon-components-svelte/src/Dropdown/Dropdown.svelte";
    import Terminal from "./Terminal.svelte";
    
    let valueDescriptions: ValueDescriptions | null = null;
    let state: State | null = null;
    let desiredState: DesiredState | null = null; 
    let modemLogOpen = false;
    let modemLog: LogLine[];
    let consoleLines: ConsoleLine[];
    let consoleCmd: string;
    let modemDbgLevel: number;

    let socket: WebSocket | null = null;
    let restartTimer: NodeJS.Timeout | null = null;
    let reconnect = true;
    let reconnectAttempts = 0;
    let maxReconnectAttempts = 10;

    let modemDbgLevels: DropdownItem[] = [
        { id: 63, text: "all" },
        { id: 32, text: "comm" },
        { id: 31, text: "debug" },
        { id: 15, text: "info" },
        { id: 7, text: "warn" },
        { id: 3, text: "error" },
        { id: 1, text: "critical" },
    ];

    function createSocket() {
        if (socket != null && socket.readyState === WebSocket.CONNECTING) {
            console.log("WebSocket already connecting, skipping...");
            return;
        }
        
        if (socket != null && socket.readyState === WebSocket.OPEN) {
            console.log("WebSocket already open, skipping...");
            return;
        }

        if (!reconnect) {
            console.log("Reconnect disabled, skipping...");
            return;
        }
        
        // Clear any existing timer
        if (restartTimer != null) {
            clearTimeout(restartTimer);
            restartTimer = null;
        }

        // Close existing socket if it exists
        if (socket != null) {
            socket.close();
            socket = null;
        }
        
        let host = location.host;
        console.log(`Attempting WebSocket connection to ws://${host}/ws (attempt ${reconnectAttempts + 1})`);
        
        try {
            socket = new WebSocket("ws://" + host + "/ws");
            
            socket.addEventListener("open", () => {
                console.log("WebSocket connected successfully");
                reconnectAttempts = 0; // Reset counter on successful connection
            });

            socket.addEventListener('close', (event) => {
                console.log(`WebSocket closed: ${event.code} ${event.reason}`);
                socket = null;
                
                if (reconnect && reconnectAttempts < maxReconnectAttempts) {
                    reconnectAttempts++;
                    const delay = Math.min(1000 * Math.pow(2, reconnectAttempts - 1), 30000); // Exponential backoff, max 30s
                    console.log(`Reconnecting in ${delay}ms (attempt ${reconnectAttempts}/${maxReconnectAttempts})`);
                    
                    restartTimer = setTimeout(() => {
                        createSocket();
                    }, delay);
                } else if (reconnectAttempts >= maxReconnectAttempts) {
                    console.error("Max reconnection attempts reached. Please refresh the page.");
                }
            });

            socket.addEventListener("error", (error) => {
                console.error("WebSocket error:", error);
                if (socket) {
                    socket.close();
                }
            });

            socket.addEventListener("message", (message: any) => {
                //console.log("message: ", message.data)
                try {
                    let jsonData = JSON.parse(message.data);
                    //console.log("parsed: ", jsonData)

                    switch (jsonData.type) {
                        case ResponseDataType.hello:
                            valueDescriptions = jsonData.data as ValueDescriptions;
                            console.log("valueDescriptions", valueDescriptions)
                            modemDbgLevel = valueDescriptions.logLevel;
                            break;
                        case ResponseDataType.mowerState:
                            state = jsonData.data as State;
                            console.log("state", state)
                            break
                        case ResponseDataType.desiredState:
                            desiredState = jsonData.data as DesiredState;
                            console.log("desiredState", desiredState)
                            break
                        case ResponseDataType.modemLog:
                            modemLog = (jsonData.data as ModemLog).log;
                            break
                        case ResponseDataType.mowerConsole:
                            consoleLines = (jsonData.data as ConsoleResponseData).lines;
                            break
                        default:
                            console.log("unknown data type");
                    }
                } catch (error) {
                    console.error("Failed to parse WebSocket message:", error);
                }
            });
        } catch (error) {
            console.error("Failed to create WebSocket:", error);
            reconnectAttempts++;
            if (reconnect && reconnectAttempts < maxReconnectAttempts) {
                const delay = Math.min(1000 * Math.pow(2, reconnectAttempts - 1), 30000);
                console.log(`Retrying in ${delay}ms`);
                restartTimer = setTimeout(() => {
                    createSocket();
                }, delay);
            }
        }
    }
    
    onMount(async () => { createSocket(); });

    onDestroy(() => {
        reconnect = false; // Disable reconnection first
        
        if (restartTimer != null) {
            clearTimeout(restartTimer);
            restartTimer = null;
        }

        if (socket != null) {
            if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
                socket.close();
            }
            socket = null;
        }
    });

    const sendText = (text:string) => {
        if ((socket != null) && (socket.readyState == socket.OPEN)) {
            socket.send(text);
        }
    }

    function handleOutputDone() {
        consoleLines = [];
    }

    $: { if (socket != null && socket.readyState == socket.OPEN) {
            let settings: RequestSocketMessage = {
                type: RequestDataType.modemLogSettings,
                data: { logLevel: modemDbgLevel } as ModemLogSettings
            };

            socket.send(JSON.stringify(settings));            
        } 
    } 

    $: { if (socket != null && socket.readyState == socket.OPEN) {
            let req: RequestSocketMessage = {
                type: RequestDataType.mowerConsoleRequest,
                data: { cmd: consoleCmd } as ConsoleRequestData
            };

            socket.send(JSON.stringify(req));
        }
    }
</script>

{#if valueDescriptions != null}
    <StateCard state={state} desiredState={desiredState} valueDescriptions={valueDescriptions}/>
{/if}

<Accordion>
{#if valueDescriptions != null}
    <AccordionItem title="Modem Log" bind:open={modemLogOpen}>
        <Console logLevels={LogLevelDesc} dbgLevels={modemDbgLevels} logData={modemLog} bind:dbgLevel={modemDbgLevel}/>
    </AccordionItem>
    <AccordionItem title="Mower console">
        <Terminal 
            consoleLines={consoleLines} 
            bind:sendCmd={consoleCmd}
            onOutputDone={handleOutputDone}
        />
    </AccordionItem>
{/if}
</Accordion>

<style lang="scss">
    :global(.bx--accordion__item--active .bx--accordion__content) {
        display: inline;
        padding: 0;
    }
</style>