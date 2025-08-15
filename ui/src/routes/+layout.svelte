<script lang="ts">
    import "carbon-components-svelte/css/g10.css";
    import {
        Button,
        Content,
        Header,
        HeaderUtilities,
        Loading,
        SkipToContent,
    } from "carbon-components-svelte";
    //import { Router, Route, navigate } from "svelte-routing";
    import IconHelp from "carbon-icons-svelte/lib/Help.svelte";
    import IconHome from "carbon-icons-svelte/lib/Home.svelte";
    //import IconMap from "carbon-icons-svelte/lib/Map.svelte";
    import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";

    import { Busy } from "../stores/busy";
    import SaveDiscard from "../widget/SaveDiscard.svelte";
    import Toasts from "../widget/Toasts.svelte";
    import { ToastNotification } from 'carbon-components-svelte';
    import { toastStore } from '../stores/toast';
    import type { ToastData } from '../stores/toast';
    let toast = $state<ToastData | null>(null);
    $effect(() => { toast = $toastStore });
    import HelpDialog from "../widget/HelpDialog.svelte";

    let { children } = $props();
    let helpOpen = $state(false);

    const help = () => (helpOpen = true);
    let url: string;
</script>

{#if $Busy}
<Loading />
{/if}
<Header company="ArduMower" platformName="Modem">
    <Button
        href="/"
        kind="tertiary"
        icon={IconHome}>
        <!-- <div class="big">Dashboard</div> -->
    </Button>
    <!-- <Button
        on:click={() => { url = "/map"; navigate(url, { replace: true }); }}
        kind="tertiary"
        icon={IconMap}>
        Map
    </Button> -->
    <div slot="skip-to-content">
        <SkipToContent />
    </div>

    <HeaderUtilities>
            <SaveDiscard />
            <Button
            href="/settings"
            kind="tertiary"
            icon={IconSettings}
            iconDescription="Settings"
            />
            <Button
            on:click={help}
            kind="tertiary"
            icon={IconHelp}
            iconDescription="Help"
            />
            <Toasts />
    </HeaderUtilities>

    {#if toast}
        <ToastNotification
            title={toast.type === 'success' ? 'Erfolg' : 'Fehler'}
            subtitle={toast.msg}
            kind={toast.type}
            timeout={4000}
            on:close={() => toastStore.set(null)}
            style="position: fixed; right: 2rem; bottom: 2rem; z-index: 9999;"
        />
    {/if}
</Header>
<Content>
    {@render children()}    
</Content>
<HelpDialog bind:open={helpOpen} />

<style lang="scss">

@media only screen and ( max-width: 500px ) {
    :global(#main-content) {
        padding: 0;
    }
}

:global(.big) {
    display: none;
}
</style>
