<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import { ConsoleLog, DataType, DesiredState, State, ValueDescriptions } from "../../model";
    import Console from "./Console.svelte";
    import { Accordion } from "carbon-components-svelte";
 
    let valueDescriptions: ValueDescriptions = null;
    let state: State = null;
    let desiredState: DesiredState = null; 
    let modemLog: any;

    let socket: WebSocket = null;
    let reconnect = true;

    function createSocket() {
        let host = location.hostname;
        socket = new WebSocket("ws://" + (((host == "[::1]") || (host == "127.0.0.1")) ? "192.168.43.186" : host) + "/ws")
        socket.addEventListener("open", ()=> {
            console.log("socket open");
        });

        socket.addEventListener('close', () => {
            console.log("socket close");
            if (reconnect) { 
                createSocket();
            } else {
                socket = null;  
            }
        });

        socket.addEventListener("message", (message: any) => {
            //console.log("message: ", message.data)
            try {
                let jsonData = JSON.parse(message.data);
                //console.log("parsed: ", jsonData)

                switch (jsonData.type) {
                    case DataType.hello:
                        valueDescriptions = jsonData.data as ValueDescriptions;
                        break;
                    case DataType.mowerState:
                        state = jsonData.data as State;
                        break
                    case DataType.desiredState:
                        desiredState = jsonData.data as DesiredState;
                        break
                    case DataType.modemLog:
                        modemLog = jsonData.data as ConsoleLog;
                        break
                    default:
                        console.log("unknown data type");
                }
            } catch {
                console.log("can't parse");
            }
        });

        setTimeout(() => {
            if ((socket != null) && (socket.readyState != 1))
                createSocket();
        }, 5000);
    }
    
    onMount(async () => { createSocket(); });

    onDestroy(() => { 
        console.log("onDestroy");    
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
</script>

<Accordion>
{#if valueDescriptions != null}
    <StateCard bind:state={state} bind:desiredState={desiredState} bind:valueDescriptions={valueDescriptions}/>
    <Console bind:logData={modemLog}/>
{/if}
</Accordion>
