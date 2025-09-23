<script lang="ts">
  import type { Settings } from "../model";
  import CheckboxSetting from "../widget/CheckboxSetting.svelte";
  import TextSetting from "../widget/TextSetting.svelte";

  export let settings: Settings.Prometheus
  export let original: Settings.Prometheus
</script>

<div class="prometheus-settings">
  <CheckboxSetting 
    label="Enabled" 
    key="prometheus.enabled"
    bind:value={settings.enabled} 
    bind:original={original.enabled} 
  />
</div>

<style lang="scss">
  .prometheus-settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }
</style>
