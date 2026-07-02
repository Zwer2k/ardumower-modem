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
    import IconHelp from "carbon-icons-svelte/lib/Help.svelte";
    import IconHome from "carbon-icons-svelte/lib/Home.svelte";
    import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";
    import IconLog from "carbon-icons-svelte/lib/DocumentView.svelte";
    import IconTerminal from "carbon-icons-svelte/lib/Terminal.svelte";
    import IconDashboard from "carbon-icons-svelte/lib/Dashboard.svelte";
    import IconMap from "carbon-icons-svelte/lib/Map.svelte";
    import IconGps from "carbon-icons-svelte/lib/Location.svelte";
    import IconTest from "carbon-icons-svelte/lib/Tools.svelte";

    import { Busy } from "../stores/busy";
    import SaveDiscard from "../widget/SaveDiscard.svelte";
    import Toasts from "../widget/Toasts.svelte";
    import { ToastNotification } from 'carbon-components-svelte';
    import { toastStore } from '../stores/toast';
    import type { ToastData } from '../stores/toast';
    import { socketService } from '../stores/socket';
    import { onDestroy, onMount } from 'svelte';
    import { browser } from '$app/environment';
    import { SettingsService } from '../service';
    import { socketStore } from '../stores/socket';

    let toast = $state<ToastData | null>(null);
    $effect(() => { toast = $toastStore });
    import HelpDialog from "../widget/HelpDialog.svelte";
    import RemoteControlPopover from "../pages/dashboard/status/RemoteControlPopover.svelte";
    import HeaderOverflowMenu from "../widget/HeaderOverflowMenu.svelte";

    let { children } = $props();
    let helpOpen = $state(false);

    let opPct = $state(0);
    let opMsg = $state('');
    let opActive = $state(false);
    let progressBusy = $derived(opActive && opPct < 100);

    $effect(() => {
        const state = $socketStore?.state;
        opPct = state?.progressPct || 0;
        opMsg = state?.progressMsg || '';
        opActive = !!state?.progressOp;
    });

    const help = () => (helpOpen = true);
    let url: string;

    onMount(() => {
        if (browser) {
            socketService.connect();
            SettingsService.init();
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
        href="/?dashboard=main"
        kind="tertiary"
        icon={IconDashboard}
        iconDescription="Dashboard">
        <span class="button-text">Dashboard</span>
    </Button>
    <Button
        href="/?dashboard=status"
        kind="tertiary"
        icon={IconDashboard}
        iconDescription="Status">
        <span class="button-text">Status</span>
    </Button>
    {#if import.meta.env.VITE_ENABLE_MAP === 'true'}
    <Button
        href="/?dashboard=map"
        kind="tertiary"
        icon={IconMap}
        iconDescription="Map">
        <span class="button-text">Map</span>
    </Button>
    {/if}
    {#if import.meta.env.VITE_ENABLE_LIVE_MAP === 'true'}
    <Button
        href="/?dashboard=livemap"
        kind="tertiary"
        icon={IconMap}
        iconDescription="Live Map">
        <span class="button-text">Live Map</span>
    </Button>
    {/if}
    {#if import.meta.env.VITE_ENABLE_GPS_DASHBOARD === 'true'}
    <Button
        href="/?dashboard=gps"
        kind="tertiary"
        icon={IconGps}
        iconDescription="GPS">
        <span class="button-text">GPS</span>
    </Button>
    {/if}

    <div slot="skip-to-content">
        <SkipToContent />
    </div>

    <HeaderUtilities>
        <RemoteControlPopover />
        <SaveDiscard />
        <HeaderOverflowMenu onHelp={help} />
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
{#if progressBusy}
    <div class="progress-bar-container">
        <div class="progress-bar {opPct === 0 ? 'indeterminate' : ''}" style="width: {opPct > 0 ? opPct + '%' : '0%'}"></div>
        <div class="progress-bar-label">{opPct > 0 ? opPct + '%' : ''} {opMsg}</div>
    </div>
{/if}
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

.progress-bar-container {
    position: fixed;
    top: 3rem;
    left: 0;
    right: 0;
    height: 18px;
    background: #e0e0e0;
    overflow: hidden;
    z-index: 9000;
    display: flex;
    align-items: center;
    justify-content: center;
}

.progress-bar {
    position: absolute;
    top: 0;
    left: 0;
    height: 100%;
    background: #4caf50;
    transition: width 0.5s ease;
    z-index: 9001;
}

.progress-bar-label {
    position: relative;
    z-index: 9002;
    font-size: 12px;
    font-weight: 600;
    color: #161616;
    white-space: nowrap;
    text-shadow: 0 0 2px rgba(255, 255, 255, 0.8);
}

.progress-bar.indeterminate {
    width: 100% !important;
    background: linear-gradient(90deg, transparent, #66bb6a, #4caf50, #66bb6a, transparent);
    background-size: 200% 100%;
    animation: progress-indeterminate 1.5s ease-in-out infinite;
}

@keyframes progress-indeterminate {
    0%   { background-position: 200% 0; }
    100% { background-position: -200% 0; }
}
</style>
