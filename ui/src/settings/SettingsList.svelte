<script lang="ts">
  import { Accordion } from "carbon-components-svelte";
  import BluetoothSettings from "./Bluetooth.svelte";
  import PS4ControllerSettings from "./PS4Controller.svelte";
  import GeneralSettings from "./General.svelte";
  import WebSettings from "./Web.svelte";
  import WiFiSettings from "./WiFi.svelte";
  import MqttSettings from "./Mqtt.svelte";
  import PrometheusSettings from "./Prometheus.svelte";
  import Firmware from "./Firmware.svelte";
  import { FrontendSettings as settings } from "../stores/frontend";
  import { BackendSettings as original } from "../stores/backend";
  import { InfoStore as info } from "../stores/info";
  import Reload from "./Reload.svelte";

  let debug;
  $: debug = JSON.stringify($settings, null, 2);
</script>

<Reload/>
{#if $settings}
  <div class="settings">
    <Accordion>
        <GeneralSettings
          bind:settings={$settings.general}
          bind:original={$original.general}
        />
        <WebSettings
          bind:settings={$settings.web}
          bind:original={$original.web}
        />
        <WiFiSettings
          bind:settings={$settings.wifi}
          bind:original={$original.wifi}
        />
        <BluetoothSettings
          bind:settings={$settings.bluetooth}
          bind:original={$original.bluetooth}
          bind:ps4ControllerSettings={$settings.ps4controller}
        />
        {#if !$settings.bluetooth.enabled && $settings.ps4controller && $original.ps4controller}
          <PS4ControllerSettings
            bind:settings={$settings.ps4controller}
            bind:original={$original.ps4controller}
            bind:bluetoothSettings={$settings.bluetooth}
            bind:info={$info}
          />
        {/if}
        <MqttSettings
          bind:settings={$settings.mqtt}
          bind:original={$original.mqtt}
          bind:allSettings={$settings}
        />
        <PrometheusSettings
          bind:settings={$settings.prometheus}
          bind:original={$original.prometheus}
        />
        <Firmware />
    </Accordion>
  </div>
{/if}
  <!-- <pre>
    {debug}
  </pre> -->

<style lang="scss">
  .settings {
    // Forciere eine stabile Layout-Basis
    position: relative;
    min-height: 100vh;
    
    :global(.bx--accordion) {
      // Verhindere abrupte Größenänderungen des Accordions
      min-height: 500px;
      
      // Isoliere Layout-Berechnungen
      contain: layout;
      
      // Stabilisiere die Accordion-Höhe
      display: block !important;
    }
    
    :global(.bx--accordion__item) {
      // Jedes Item hat eine stabile Basis-Höhe
      min-height: 60px;
      contain: layout style;
      
      // Verhindere Layout-Sprünge
      position: relative;
    }
    
    :global(.bx--accordion__content) {
      padding: 0 0.5rem 1rem 0.5rem;
      
      // Keine Transition - verhindert Konflikte
      transition: none !important;
      
      // Stable rendering
      contain: layout style;
      transform: translateZ(0);
    }

    :global(.bx--form-item) {
        margin-top: 0.5rem;
        // Stable layout für Form-Items
        contain: layout style;
        
        // Verhindere Reflow
        will-change: auto;
    }

    :global(.bx--label) {
        margin-bottom: 0.1rem;
    }

    :global(.bx--toggle-input__label .bx--toggle__switch) {
        margin-top: 0.1rem !important;
    }
    
    /* Force stable height calculation */
    :global(.bx--accordion__item--active .bx--accordion__content) {
      height: auto !important;
      min-height: 50px;
    }
  }
</style>