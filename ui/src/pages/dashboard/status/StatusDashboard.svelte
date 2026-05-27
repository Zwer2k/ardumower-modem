<script lang="ts">
    import StateCard from "./StateCard.svelte";
    import SensorEvents from "./SensorEvents.svelte";
    import { socketStore, socketService } from '../../../stores/socket';
    import { page } from '$app/stores';
    import { browser } from '$app/environment';

    // Reagiere auf Sichtbarkeit (Dashboard ist immer im DOM, nur CSS hidden)
    $effect(() => {
        if (!browser) return;
        const dashboard = $page.url?.searchParams?.get('dashboard');
        if (dashboard === 'status') {
            socketService.requestSensorSummary();
        } else {
            socketService.stopSensorSummary();
        }
    });
</script>

<div class="dashboard-content">
    {#if browser && $socketStore.valueDescriptions != null}
        <div class="dashboard-main">
            <StateCard
                state={$socketStore.state}
                stats={$socketStore.stats}
                desiredState={$socketStore.desiredState}
                valueDescriptions={$socketStore.valueDescriptions}
            />
            <SensorEvents summary={$socketStore.sensorSummary} />
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
        overflow-y: auto;

        /* Abzug der Header-Höhe */
        padding-top: 48px; /* Standard Carbon Header Höhe */
    }

    .dashboard-main {
        flex: 1;
        display: flex;
        flex-direction: column;
        gap: 10px;
        padding: 10px;
    }
</style>
