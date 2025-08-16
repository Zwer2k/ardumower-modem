<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { onMount, onDestroy } from 'svelte';
    import { socketStore, socketService } from '../../stores/socket';

    onMount(() => {
        socketService.connect();
    });

    onDestroy(() => {
        // Don't disconnect here as other components might be using the socket
    });
</script>

<div class="dashboard-content">
    {#if $socketStore.valueDescriptions != null}
        <StateCard 
            state={$socketStore.state} 
            desiredState={$socketStore.desiredState} 
            valueDescriptions={$socketStore.valueDescriptions}
        />
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
</style>
