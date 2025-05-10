<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import type { ConsoleLog, ConsoleLogSettings, DesiredState, LogLine, RequestSocketMessage, State, ValueDescriptions } from "../../model";
    import { LogLevelDesc, RequestDataType, ResponseDataType } from "../../model";
    import Console from "./Console.svelte";
    import { Accordion } from "carbon-components-svelte";
    import type { DropdownItem } from "carbon-components-svelte/src/Dropdown/Dropdown.svelte";
    
    let valueDescriptions: ValueDescriptions | null = null;
    let state: State | null = null;
    let desiredState: DesiredState | null = null; 
    let modemLog: LogLine[];
    let modemDbgLevel: number;

    let socket: WebSocket | null = null;
    let restartTimer: NodeJS.Timeout | null = null;
    let reconnect = true;

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
        if (socket != null)
            return;
        
        let host = location.host;
        socket = new WebSocket("ws://" + host + "/ws")
        socket.addEventListener("open", ()=> {
            console.log("socket open");
        });

        socket.addEventListener('close', () => {
            console.log("socket close");
            restartTimer = setTimeout(() => {
            if ((socket != null) && (socket.readyState != socket.OPEN)) {
                socket = null;
                createSocket();
            }
        }, 5000);
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
                        modemLog = (jsonData.data as ConsoleLog).log;
                        break
                    default:
                        console.log("unknown data type");
                }
            } catch {
                console.log("can't parse");
            }
        });

        // if (restartTimer != null) {
        //     clearInterval(restartTimer);
        // }

        // restartTimer = setInterval(() => {
        //     if ((socket == null) || (socket.readyState != socket.OPEN)) {
        //         socket = null;
        //         createSocket();
        //     }
        // }, 5000);
    }
    
    onMount(async () => { createSocket(); });

    onDestroy(() => {
        if (restartTimer != null) {
            clearInterval(restartTimer);
        }

        if ((socket != null) && (socket.readyState == socket.OPEN)) {
            socket.close();
            socket = null;
        }    
        reconnect = false;
    });

    const sendText = (text:string) => {
        if ((socket != null) && (socket.readyState == socket.OPEN)) {
            socket.send(text);
        }
    }

    $: { if (socket != null && socket.readyState == socket.OPEN) {
            let settings: RequestSocketMessage = {
                type: RequestDataType.modemLogSettings,
                data: { logLevel: modemDbgLevel } as ConsoleLogSettings
            };

            socket.send(JSON.stringify(settings));            
        } 
    } 
</script>

<Accordion>
{#if valueDescriptions != null}
    <StateCard state={state} desiredState={desiredState} valueDescriptions={valueDescriptions}/>
    <Console logLevels={LogLevelDesc} dbgLevels={modemDbgLevels} logData={modemLog} bind:dbgLevel={modemDbgLevel}/>
{/if}
</Accordion>
