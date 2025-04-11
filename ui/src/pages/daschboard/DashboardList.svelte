<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import { ConsoleLog, ConsoleLogSettings, DesiredState, LogLevelDesc, LogLine, RequestDataType, RequestSocketMessage, ResponseDataType, State, ValueDescriptions } from "../../model";
    import Console from "./Console.svelte";
    import { Accordion } from "carbon-components-svelte";
    import type { DropdownItem } from "carbon-components-svelte/types/Dropdown/Dropdown.svelte";
 
    let valueDescriptions: ValueDescriptions = null;
    let state: State = null;
    let desiredState: DesiredState = null; 
    let modemLog: LogLine[];
    let modemDbgLevel: number;

    let socket: WebSocket = null;
    let restartTimer = null;
    let reconnect = true;

    let modemDbgLevels: DropdownItem[] = [
        { id: "63", text: "all" },
        { id: "32", text: "comm" },
        { id: "31", text: "debug" },
        { id: "15", text: "info" },
        { id: "7", text: "warn" },
        { id: "3", text: "error" },
        { id: "1", text: "critical" },
    ];

    function createSocket() {
        if (socket != null)
            return;
        
        let host = location.hostname;
        socket = new WebSocket("ws://" + (((host == "[::1]") || (host == "127.0.0.1") || (host == "localhost")) ? "192.168.43.173" : host) + "/ws")
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
        console.log("onDestroy");    
        if (restartTimer != null) {
            clearInterval(restartTimer);
        }

        if ((socket != null) && (socket.readyState == socket.OPEN)) {
            socket.close();
            socket = null;
        }    
        reconnect = false;
    });

    const sendText = (text) => {
        if (socket.readyState == socket.OPEN) {
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
