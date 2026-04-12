<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import { page } from '$app/stores';
    import { browser } from '$app/environment';
    import { socketService, socketStore } from '../../stores/socket';
    import StatusDashboard from './StatusDashboard.svelte';
    import LogDashboard from './LogDashboard.svelte';
    import TerminalDashboard from './TerminalDashboard.svelte';
    import Map from '../../map/Map.svelte';

    let currentDashboard = $state('status');

    // Reagiere auf URL-Änderungen (Query-Parameter basiert)
    $effect(() => {
        if (browser && $page.url) {
            const dashboard = $page.url.searchParams.get('dashboard') || 'status';
            if (['status', 'log', 'terminal', 'map'].includes(dashboard)) {
                currentDashboard = dashboard;
            }
        }
    });

    onMount(() => {
        if (browser) {
            document.body.classList.add('dashboard-mode');
            console.log('DashboardContainer: Dashboard mode activated');
        }
    });

    onDestroy(() => {
        if (browser) {
            document.body.classList.remove('dashboard-mode');
            console.log('DashboardContainer: Cleanup');
        }
    });
</script>

<div class="dashboard-container">
    <!-- Alle Dashboards sind immer geladen und bleiben im Memory -->
    <div class="dashboard-panel" class:active={currentDashboard === 'status'}>
        <StatusDashboard />
    </div>
    <div class="dashboard-panel" class:active={currentDashboard === 'log'}>
        <LogDashboard />
    </div>
    <div class="dashboard-panel" class:active={currentDashboard === 'terminal'}>
        <TerminalDashboard />
    </div>
    <div class="dashboard-panel" class:active={currentDashboard === 'map'}>
        <Map />
    </div>
</div>

<style>
    .dashboard-container {
        position: relative;
        width: 100%;
        height: 100vh;
        overflow: hidden;
    }

    .dashboard-panel {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        opacity: 0;
        visibility: hidden;
        transition: opacity 0.2s ease-in-out;
        pointer-events: none;
    }

    .dashboard-panel.active {
        opacity: 1;
        visibility: visible;
        pointer-events: auto;
    }

</style>
