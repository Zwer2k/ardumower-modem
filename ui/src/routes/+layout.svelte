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
    import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";
    import IconLog from "carbon-icons-svelte/lib/DocumentView.svelte";
    import IconTerminal from "carbon-icons-svelte/lib/Terminal.svelte";
    import IconDashboard from "carbon-icons-svelte/lib/Dashboard.svelte";
    import IconMap from "carbon-icons-svelte/lib/Map.svelte";

    import { Busy } from "../stores/busy";
    import SaveDiscard from "../widget/SaveDiscard.svelte";
    import Toasts from "../widget/Toasts.svelte";
    import { ToastNotification } from 'carbon-components-svelte';
    import { toastStore } from '../stores/toast';
    import type { ToastData } from '../stores/toast';
    import { socketService } from '../stores/socket';
    import { onDestroy, onMount } from 'svelte';
    import { browser } from '$app/environment';
    let toast = $state<ToastData | null>(null);
    $effect(() => { toast = $toastStore });
    import HelpDialog from "../widget/HelpDialog.svelte";

    let { children } = $props();
    let helpOpen = $state(false);

    const help = () => (helpOpen = true);
    let url: string;

    onMount(() => {
        if (browser) {
            socketService.connect();
        }
    });

    onDestroy(() => {
        socketService.destroy();
    });
</script>

{#if $Busy}
<Loading />
{/if}
<Header company="ArduMower" platformName="Modem">
    <Button
        href="/?dashboard=status"
        kind="tertiary"
        icon={IconDashboard}
        iconDescription="Status">
        <span class="button-text">Status</span>
    </Button>
    <Button
        href="/?dashboard=map"
        kind="tertiary"
        icon={IconMap}
        iconDescription="Map">
        <span class="button-text">Map</span>
    </Button>
    <Button
        href="/?dashboard=log"
        kind="tertiary"
        icon={IconLog}
        iconDescription="Modem Log">
        <span class="button-text">Log</span>
    </Button>
    <Button
        href="/?dashboard=terminal"
        kind="tertiary"
        icon={IconTerminal}
        iconDescription="Terminal">
        <span class="button-text">Terminal</span>
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

/* Header-Name (ArduMower Modem) auf kleinen Bildschirmen ausblenden */
@media only screen and (max-width: 768px) {
    :global(.bx--header__name) {
        display: none !important;
    }
    /* Buttons ganz links anordnen */
    :global(.bx--header__nav) {
        margin-left: 0 !important;
    }
    :global(.bx--header .bx--btn--tertiary) {
        margin-left: 0 !important;
    }
}

/* Icon-Größe in der Header-Navigation vergrößern */
:global(.bx--header__action svg) {
    width: 20px !important;
    height: 20px !important;
}

/* Button-Text auf kleinen Bildschirmen ausblenden */
@media only screen and (max-width: 768px) {
    .button-text {
        display: none;
    }
    
    /* Navigation-Buttons genauso kompakt wie HeaderUtilities-Buttons machen */
    :global(.bx--header .bx--btn--tertiary) {
        min-width: 48px !important;
        width: 48px !important;
        padding: 0 !important;
        justify-content: center !important;
    }
    
    /* Icon-Container zentrieren */
    :global(.bx--header .bx--btn--tertiary .bx--btn__icon) {
        margin: 0 !important;
    }
    
    /* Entferne Text-Spacing */
    :global(.bx--header .bx--btn--tertiary:not(.bx--header__action)) {
        padding-left: 0 !important;
        padding-right: 0 !important;
    }
}

@media only screen and (max-width: 500px) {
    :global(#main-content) {
        padding: 0;
    }
    
    /* Noch kleinere Bildschirme - Icons etwas kleiner aber immer noch größer als Standard */
    :global(.bx--header__action svg) {
        width: 18px !important;
        height: 18px !important;
    }
}

/* Globaler Reset */
:global(html) {
    margin: 0;
    padding: 0;
    height: 100%;
    overflow: hidden;
}

:global(body) {
    margin: 0;
    padding: 0;
    height: 100%;
    overflow: hidden;
}

:global(#main-content) {
    position: relative;
}

/* Dashboard-Modus: Komplett ohne Scrollbars */
:global(body.dashboard-mode) {
    overflow: hidden !important;
    height: 100vh !important;
}

:global(body.dashboard-mode .bx--content) {
    padding: 0 !important;
    margin: 0 !important;
    height: 100vh !important;
    overflow: hidden !important;
}

:global(.dashboard-page) {
    height: 100vh !important;
    overflow: hidden !important;
}

/* Standard-Layout für Settings/andere Seiten - Nur Content scrollt */
:global(body:not(.dashboard-mode) .bx--content) {
    padding: 1rem !important;
    height: calc(100vh - 48px) !important;
    overflow-y: auto !important;
    overflow-x: hidden !important;
}

:global(.big) {
    display: none;
}
</style>
