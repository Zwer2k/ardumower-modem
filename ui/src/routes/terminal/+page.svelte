<script>
    import TerminalDashboard from "../../pages/daschboard/TerminalDashboard.svelte";
    import { onMount, onDestroy } from 'svelte';

    onMount(() => {
        document.body.classList.add('dashboard-mode');
    });

    onDestroy(() => {
        document.body.classList.remove('dashboard-mode');
    });
</script>

<div class="dashboard-page">
    <TerminalDashboard />
</div>
