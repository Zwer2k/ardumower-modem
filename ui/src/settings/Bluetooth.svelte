<script lang="ts">
  import type { ChangeEventValue, Settings } from "../model";
  import CheckboxSetting from "../widget/CheckboxSetting.svelte";
  import TextSetting from "../widget/TextSetting.svelte";
  import BluetoothClean from "./BluetoothClean.svelte";

  import { tick } from 'svelte';

  export let settings: Settings.Bluetooth;
  export let original: Settings.Bluetooth;
  export let ps4ControllerSettings: Settings.PS4Controller;

  const enableChange = async (ev: ChangeEventValue) => {
    // Warte einen Tick, damit die Bluetooth-Transition abgeschlossen ist
    await tick();
    ps4ControllerSettings.enabled = false;
  }
</script>

<div class="bluetooth-settings">
  <CheckboxSetting
    label="Control and monitor your ArduMower with Bluetooth"
    key="bluetooth.enabled"
    bind:value={settings.enabled}
    bind:original={original.enabled}
    on:change={(e) => enableChange(e.detail)}
  />
  
  {#if settings.enabled}
    <div class="enabled-settings">
      <CheckboxSetting
        label="Bluetooth pairings require a PIN code"
        key="bluetooth.pin_enabled"
        bind:value={settings.pin_enabled}
        bind:original={original.pin_enabled}
      />

      <TextSetting
        label="PIN"
        key="bluetooth.pin"
        placeholder="Bluetooth pairing PIN"
        kind="password"
        disabled={!settings.pin_enabled}
        bind:value={settings.pin}
        bind:original={original.pin}
      />

      <BluetoothClean
        bind:bluetoothSettings={settings}
        bind:ps4ControllerSettings={ps4ControllerSettings}
      />
    </div>
  {/if}
</div>

<style lang="scss">
  .bluetooth-settings {
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
