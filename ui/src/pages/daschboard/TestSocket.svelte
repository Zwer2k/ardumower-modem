<script>
    import { writable } from 'svelte/store';

    const messageStore = writable('');

    const socket = new WebSocket('ws://192.168.43.173/ws');

    // Connection opened
    socket.addEventListener('open', function (event) {
        console.log("It's open");
        sendMessage("Hallo");
    });

    // Listen for messages
    socket.addEventListener('message', function (event) {
        messageStore.set(event.data);
        console.log("message: ", event);
    });

    const sendMessage = (message) => {
        if (socket.readyState <= 1) {
            socket.send(message);
        }
    }
</script>

<div></div>