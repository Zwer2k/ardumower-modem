<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import { ConsoleLog, ConsoleLogSettings, DesiredState, RequestDataType, RequestSocketMessage, ResponseDataType, State, ValueDescriptions } from "../../model";
    import Console, { LogLevelItem } from "./Console.svelte";
    import { Accordion } from "carbon-components-svelte";
 
    let valueDescriptions: ValueDescriptions = null;
    let state: State = null;
    let desiredState: DesiredState = null; 
    let modemLog: ConsoleLog;
    let modemLogLevel: number;

    let socket: WebSocket = null;
    let restartTimer = null;
    let reconnect = true;

    let modemLogLevels: LogLevelItem[] = [
        { id: "0", text: "none" },
        { id: "31", text: "debug" },
        { id: "15", text: "info" },
        { id: "7", text: "error" },
        { id: "3", text: "emergency" },
        { id: "1", text: "critical" },
    ];

    function createSocket() {
        if (socket != null)
            return;
        
        let host = location.hostname;
        socket = new WebSocket("ws://" + (((host == "[::1]") || (host == "127.0.0.1")) ? "192.168.43.186" : host) + "/ws")
        socket.addEventListener("open", ()=> {
            console.log("socket open");
        });

        socket.addEventListener('close', () => {
            console.log("socket close");
            socket = null;
            if (reconnect) { 
                createSocket();
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
                        break;
                    case ResponseDataType.mowerState:
                        state = jsonData.data as State;
                        break
                    case ResponseDataType.desiredState:
                        desiredState = jsonData.data as DesiredState;
                        break
                    case ResponseDataType.modemLog:
                        modemLog = jsonData.data as ConsoleLog;
                        break
                    default:
                        console.log("unknown data type");
                }
            } catch {
                console.log("can't parse");
            }
        });

        if (restartTimer != null) {
            clearInterval(restartTimer);
        }

        restartTimer = setInterval(() => {
            if ((socket == null) || (socket.readyState != socket.OPEN)) {
                socket = null;
                createSocket();
            }
        }, 5000);
    }
    
    onMount(async () => { createSocket(); });

    onDestroy(() => { 
        console.log("onDestroy");    
        if (restartTimer != null) {
            clearInterval(restartTimer);
        }

        if ((socket != null) && (socket.readyState == socket.OPEN)) {
            socket.close();
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
                data: { logLevel: modemLogLevel } as ConsoleLogSettings
            };

            socket.send(JSON.stringify(settings));
        } 
    } 
</script>

<Accordion>
{#if valueDescriptions != null}
    <StateCard state={state} desiredState={desiredState} valueDescriptions={valueDescriptions}/>
    <Console logLevels={modemLogLevels} logData={modemLog} bind:logLevel={modemLogLevel}/>
{/if}
</Accordion>
