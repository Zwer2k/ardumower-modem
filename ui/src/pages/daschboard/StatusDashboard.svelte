<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import { socketStore } from '../../stores/socket';
    import { browser } from '$app/environment';
</script>

<div class="dashboard-content">
    {#if browser && $socketStore.valueDescriptions != null}
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
