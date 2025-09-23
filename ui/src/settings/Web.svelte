<script lang="ts">
  import TextSetting from "../widget/TextSetting.svelte";
  import CheckboxSetting from "../widget/CheckboxSetting.svelte";
  import type { Settings } from "../model";

  export let settings: Settings.Web;
  export let original: Settings.Web;
</script>

<div class="web-settings">
  <CheckboxSetting
    label="Protect this web interface"
    key="web.protected"
    bind:value={settings.protected}
    bind:original={original.protected}
  />

  {#if settings.protected}
    <div class="enabled-settings">
      <TextSetting
        label="Username"
        key="web.username"
        bind:value={settings.username}
        bind:original={original.username}
      />
      <TextSetting
        label="Password"
        key="web.password"
        kind="password"
        bind:value={settings.password}
        bind:original={original.password}
      />
    </div>
  {/if}
</div>

<style lang="scss">
  .web-settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .enabled-settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
    margin-left: 1.5rem;
    padding-left: 1rem;
    border-left: 2px solid var(--cds-border-subtle-01);
  }
</style>
