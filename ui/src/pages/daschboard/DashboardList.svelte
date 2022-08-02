<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount } from 'svelte';
    import { DataType, SocketMessage, State } from "../../model";

    let state: State = null;

    let socket;
    onMount(async () => {
        socket = new WebSocket("ws://192.168.43.173/ws")
        socket.addEventListener("open", ()=> {
            console.log("Opened");
            sendMessage({ type: 1, data: { abc: 1 }});
        });

        socket.addEventListener('close', function (event) {
            console.log("It's close");
        });

        socket.addEventListener("message", (message: any) => {
            console.log("message: ", message.data)
            try {
                let jsonData = JSON.parse(message.data);
                console.log("parsed: ", jsonData)

                switch (jsonData.type) {
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
    });

    const sendMessage = (message) => {
        if (socket.readyState <= 1) {
            socket.send(JSON.stringify(message));
        }
    }
</script>

<StateCard bind:state={state}/>