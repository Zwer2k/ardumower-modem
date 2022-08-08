<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount } from 'svelte';
    import { DataType, SocketMessage, State, ValueDescriptions } from "../../model";

    let valueDescriptions: ValueDescriptions = null;
    let state: State = null;

    let socket;
    function createSocket() {
        let host = location.hostname;
        socket = new WebSocket("ws://" + (((host == "[::1]") || (host == "127.0.0.1")) ? "192.168.43.186" : host) + "/ws")
        socket.addEventListener("open", ()=> {
            console.log("socket open");
        });

        socket.addEventListener('close', () => {
            console.log("socket close");
            createSocket();
        });

        socket.addEventListener("message", (message: any) => {
            //console.log("message: ", message.data)
            try {
                let jsonData = JSON.parse(message.data);
                console.log("parsed: ", jsonData)

                switch (jsonData.type) {
                    case DataType.hello:
                        valueDescriptions = jsonData.data as ValueDescriptions;
                        break;
                    case DataType.mowerState:
                        state = jsonData.data as State;
                        break
                    default:
                        console.log("unknown data type");
                }
            } catch {
                console.log("can't parse");
            }
        });

        setTimeout(() => {
            if (socket.readyState != 1)
                createSocket();
        }, 5000);
    }
    
    onMount(async () => { createSocket(); });

    const sendMessage = (message) => {
        if (socket.readyState <= 1) {
            socket.send(JSON.stringify(message));
        }
    }
</script>

<StateCard bind:state={state} bind:valueDescriptions={valueDescriptions}/>