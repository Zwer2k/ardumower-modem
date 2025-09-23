<script lang="ts">
  import type { Settings } from "../model";
  import CheckboxSetting from "../widget/CheckboxSetting.svelte";
  import TextSetting from "../widget/TextSetting.svelte";

  export let settings: Settings.General;
  export let original: Settings.General;
</script>

<div class="general-settings">
  <TextSetting
    label="Device name"
    key="general.name"
    helpText="The name is used during Bluetooth discovery, as WiFi hostname, MQTT topic, etc."
    bind:value={settings.name}
    bind:original={original.name}
  />
  <CheckboxSetting
    label="ArduMower Encryption"
    key="general.encryption"
    bind:value={settings.encryption}
    bind:original={original.encryption}
  />
  <TextSetting
    label="ArduMower encryption password"
    key="general.password"
    helpText="The password configured in config.h"
    kind="password"
    disabled={!settings.encryption}
    bind:value={settings.password}
    bind:original={original.password}
  />
</div>

<style lang="scss">
  .general-settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }
</style>
