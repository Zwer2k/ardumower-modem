<script lang="ts">
  import "carbon-components-svelte/css/g10.css";
  import {
    Content,
    Button,
    Header,
    HeaderUtilities,
    Loading,
    SkipToContent,
  } from "carbon-components-svelte";
  import { Router, Route, navigate } from "svelte-routing";
  import IconHelp from "carbon-icons-svelte/lib/Help16";
  import IconHome from "carbon-icons-svelte/lib/Home16";
  import IconMap from "carbon-icons-svelte/lib/Map16";
  import IconSettings from "carbon-icons-svelte/lib/Settings16";

  import DashboardRoute from "./routes/Dashboard.svelte";
  import MapRoute from "./routes/Map.svelte";
  import SettingsRoute from "./routes/Settings.svelte";
  import { Busy } from "./stores/busy";
  import SaveDiscard from "./widget/SaveDiscard.svelte";
  import Toasts from "./widget/Toasts.svelte";
  import HelpDialog from "./widget/HelpDialog.svelte";

  let helpOpen = false;

  const help = () => (helpOpen = true);
  export let url = "/";
</script>

{#if $Busy}
  <Loading />
{/if}
<Header company="ArduMower" platformName="Modem">
  <Button
    on:click={() => { url = "/"; navigate(url, { replace: true }); }}
    kind="tertiary"
    icon={IconHome}>
    Dashboard
  </Button>
  <Button
    on:click={() => { url = "/map"; navigate(url, { replace: true }); }}
    kind="tertiary"
    icon={IconMap}>
    Map
  </Button>
  <div slot="skip-to-content">
    <SkipToContent />
  </div>

  <HeaderUtilities>
    <SaveDiscard />
    <Button
      on:click={() => { url = "/settings"; navigate(url, { replace: true }); }}  
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
  <HelpDialog bind:open={helpOpen} />
  <Router url="{url}">
    <div>
      <Route path="/"  component="{DashboardRoute}" />
      <Route path="/map" component="{MapRoute}" />
      <Route path="/settings" component="{SettingsRoute}" /> 
    </div>
  </Router>
  <DashboardRoute />
</Content>

<style>
  :global(.bx--toggle-input__label .bx--toggle__switch) {
    margin-top: 0.5rem !important;
  }
</style>