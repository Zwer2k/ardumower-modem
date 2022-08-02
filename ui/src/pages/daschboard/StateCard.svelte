<script lang="ts">
    import { onMount } from 'svelte';
    
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
            //const data: Request = JSON.parse(message.data);
            console.log("message: ", message)
        });
    });

    const sendMessage = (message) => {
        if (socket.readyState <= 1) {
            socket.send(JSON.stringify(message));
        }
    }
</script>

<article class="contact-card">
	<h3>State</h3>

	<div class="item-list">
        <div class="label">Updateted</div><div>value</div>
    </div>
</article>

<style>
	.contact-card {
		width: 100%;
		border: 1px solid #aaa;
		border-radius: 2px;
		box-shadow: 2px 2px 8px rgba(0,0,0,0.1);
		padding: 1em;
	}

	h3 {
		padding: 0 0 0.2em 0;
		margin: 0 0 1em 0;
		border-bottom: 1px solid #ff3e00
	}

	.item-list {
		display: grid;
        grid-template-columns: minmax(20%, max-content) auto;
	}

    .label {
        font-weight: bold;
        margin-right: 10px;
    }
</style>
