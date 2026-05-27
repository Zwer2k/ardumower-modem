<script lang="ts">
    import MotorPlot from "./MotorPlot.svelte";
    import { socketStore } from '../../../stores/socket';
    import { browser } from '$app/environment';
</script>

<div class="dashboard-content">
    {#if browser && $socketStore.valueDescriptions != null}
        <div class="tests-main">
            <MotorPlot />
            <!-- Platz für weitere Tests -->
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
        padding-top: 48px; /* Carbon Header */
    }

    .tests-main {
        flex: 1;
        display: flex;
        flex-direction: column;
        padding: 12px;
        width: 100%;
        height: 100%;
        box-sizing: border-box;
    }
</style>
