<script lang="ts">
    import Terminal from "./Terminal.svelte";
    import { onMount } from 'svelte';
    import { socketStore, socketService } from '../../stores/socket';
    import { getModemInfo } from '../../firmware/service';
    import { browser } from '$app/environment';
    import type { ApiModemInfoResponse } from '../../firmware/service';

    let showTerminal = false;
    let modemInfo: ApiModemInfoResponse | null = null;
    let consoleCmd: string;

    onMount(async () => {
        if (!browser) return;
        
        // Fetch /api/modem/info für terminal_available
        try {
            modemInfo = await getModemInfo();
            showTerminal = !!modemInfo?.terminal_available;
            console.log('modemInfo', modemInfo);
        } catch (e) {
            console.warn('Could not fetch /api/modem/info:', e);
        }
    });

    function handleOutputDone() {
        socketService.clearConsoleLines();
    }

    $: {
        if (browser && $socketStore.connected && consoleCmd) {
            socketService.sendConsoleCommand(consoleCmd);
        }
    }
</script>

<div class="dashboard-content">
    {#if showTerminal && $socketStore.valueDescriptions != null}
        <div class="terminal-wrapper">
            <Terminal 
                consoleLines={$socketStore.consoleLines} 
                bind:sendCmd={consoleCmd}
                onOutputDone={handleOutputDone}
            />
        </div>
    {:else if !showTerminal}
        <div class="terminal-unavailable">
            <h3>Terminal nicht verfügbar</h3>
            <p>Das Terminal ist in dieser Konfiguration nicht verfügbar.</p>
        </div>
    {/if}
</div>

<style lang="scss">
    .dashboard-content {
        width: 100%;
        height: 100vh;
        padding: 0;
        margin: 0;
        display: flex;
        flex-direction: column;
        overflow: hidden;
        
        /* Abzug der Header-Höhe */
        padding-top: 48px; /* Standard Carbon Header Höhe */
    }

    .terminal-wrapper {
        flex: 1;
        display: flex;
        flex-direction: column;
        height: calc(100vh - 48px);
        overflow: hidden;
        padding: 0;
    }

    .terminal-unavailable {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: calc(100vh - 48px);
        text-align: center;
        color: #6f6f6f;

        h3 {
            margin-bottom: 1rem;
        }
    }
</style>
