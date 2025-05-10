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
