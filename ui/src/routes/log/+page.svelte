<script>
    import LogDashboard from "../../pages/daschboard/LogDashboard.svelte";
    import { onMount, onDestroy } from 'svelte';

    onMount(() => {
        document.body.classList.add('dashboard-mode');
    });

    onDestroy(() => {
        document.body.classList.remove('dashboard-mode');
    });
</script>

<div class="dashboard-page">
    <LogDashboard />
</div>
