<script lang="ts">
    import Console from "./Console.svelte";
    import { onMount } from 'svelte';
    import { socketStore, socketService } from '../../../stores/socket';
    import { LogLevelDesc } from "../../../model";
    import { browser } from '$app/environment';
    import type { DropdownItem } from "carbon-components-svelte/src/Dropdown/Dropdown.svelte";

    let modemDbgLevel: number = 15;
    let lastSentLogLevel: number = -1; // Track last sent level to prevent loops

    let modemDbgLevels: DropdownItem[] = [
        { id: 63, text: "all" },
        { id: 32, text: "comm" },
        { id: 31, text: "debug" },
        { id: 15, text: "info" },
        { id: 7, text: "warn" },
        { id: 3, text: "error" },
        { id: 1, text: "critical" },
    ];

    // Initialize from store once when available
    $: {
        if (browser && $socketStore.valueDescriptions && lastSentLogLevel === -1) {
            modemDbgLevel = $socketStore.modemDbgLevel;
            lastSentLogLevel = $socketStore.modemDbgLevel;
        }
    }

    // Only send when user actually changes the level
    $: {
        if (browser && $socketStore.connected && modemDbgLevel !== lastSentLogLevel && lastSentLogLevel !== -1) {
            socketService.setLogLevel(modemDbgLevel);
            lastSentLogLevel = modemDbgLevel;
        }
    }
</script>

<div class="dashboard-content">
    {#if $socketStore.valueDescriptions != null}
        <div class="console-wrapper">
            <Console 
                logLevels={LogLevelDesc} 
                dbgLevels={modemDbgLevels} 
                logData={$socketStore.modemLog} 
                bind:dbgLevel={modemDbgLevel}
            />
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
    
    .console-wrapper {
        flex: 1;
        display: flex;
        flex-direction: column;
        height: calc(100vh - 48px);
        overflow: hidden;
    }
</style>
