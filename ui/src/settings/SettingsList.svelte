<script lang="ts">
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
  
  // Verwalte Accordion-Zustand selbst - ohne Carbon Accordion
  let openSections = new Set(['general', 'web', 'wifi', 'bluetooth', 'mqtt', 'prometheus', 'firmware']);
  
  function toggleSection(sectionId: string) {
    if (openSections.has(sectionId)) {
      openSections.delete(sectionId);
    } else {
      openSections.add(sectionId);
    }
    openSections = new Set(openSections); // Trigger reactivity
  }
</script>

<Reload/>
{#if $settings}
  <div class="settings">
    <div class="custom-accordion">
      
      <!-- General Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('general')}
          on:click={() => toggleSection('general')}
        >
          <span class="accordion-title">General</span>
          <span class="accordion-icon">{openSections.has('general') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('general')}
          <div class="accordion-content">
            <GeneralSettings
              bind:settings={$settings.general}
              bind:original={$original.general}
            />
          </div>
        {/if}
      </div>

      <!-- Web Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('web')}
          on:click={() => toggleSection('web')}
        >
          <span class="accordion-title">Web</span>
          <span class="accordion-icon">{openSections.has('web') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('web')}
          <div class="accordion-content">
            <WebSettings
              bind:settings={$settings.web}
              bind:original={$original.web}
            />
          </div>
        {/if}
      </div>

      <!-- WiFi Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('wifi')}
          on:click={() => toggleSection('wifi')}
        >
          <span class="accordion-title">WiFi</span>
          <span class="accordion-icon">{openSections.has('wifi') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('wifi')}
          <div class="accordion-content">
            <WiFiSettings
              bind:settings={$settings.wifi}
              bind:original={$original.wifi}
            />
          </div>
        {/if}
      </div>

      <!-- Bluetooth Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('bluetooth')}
          on:click={() => toggleSection('bluetooth')}
        >
          <span class="accordion-title">Bluetooth</span>
          <span class="accordion-icon">{openSections.has('bluetooth') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('bluetooth')}
          <div class="accordion-content">
            <BluetoothSettings
              bind:settings={$settings.bluetooth}
              bind:original={$original.bluetooth}
              bind:ps4ControllerSettings={$settings.ps4controller}
            />
          </div>
        {/if}
      </div>

      <!-- PS4 Controller Settings -->
      {#if !$settings.bluetooth.enabled && $settings.ps4controller && $original.ps4controller}
        <div class="accordion-item">
          <button 
            class="accordion-header" 
            class:open={openSections.has('ps4controller')}
            on:click={() => toggleSection('ps4controller')}
          >
            <span class="accordion-title">PS4 Controller</span>
            <span class="accordion-icon">{openSections.has('ps4controller') ? '−' : '+'}</span>
          </button>
          {#if openSections.has('ps4controller')}
            <div class="accordion-content">
              <PS4ControllerSettings
                bind:settings={$settings.ps4controller}
                bind:original={$original.ps4controller}
                bind:bluetoothSettings={$settings.bluetooth}
                bind:info={$info}
              />
            </div>
          {/if}
        </div>
      {/if}

      <!-- MQTT Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('mqtt')}
          on:click={() => toggleSection('mqtt')}
        >
          <span class="accordion-title">MQTT</span>
          <span class="accordion-icon">{openSections.has('mqtt') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('mqtt')}
          <div class="accordion-content">
            <MqttSettings
              bind:settings={$settings.mqtt}
              bind:original={$original.mqtt}
              bind:allSettings={$settings}
            />
          </div>
        {/if}
      </div>

      <!-- Prometheus Settings -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('prometheus')}
          on:click={() => toggleSection('prometheus')}
        >
          <span class="accordion-title">Prometheus</span>
          <span class="accordion-icon">{openSections.has('prometheus') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('prometheus')}
          <div class="accordion-content">
            <PrometheusSettings
              bind:settings={$settings.prometheus}
              bind:original={$original.prometheus}
            />
          </div>
        {/if}
      </div>

      <!-- Firmware -->
      <div class="accordion-item">
        <button 
          class="accordion-header" 
          class:open={openSections.has('firmware')}
          on:click={() => toggleSection('firmware')}
        >
          <span class="accordion-title">Firmware</span>
          <span class="accordion-icon">{openSections.has('firmware') ? '−' : '+'}</span>
        </button>
        {#if openSections.has('firmware')}
          <div class="accordion-content">
            <Firmware />
          </div>
        {/if}
      </div>

    </div>
  </div>
{/if}
  <!-- <pre>
    {debug}
  </pre> -->

<style lang="scss">
  .settings {
    max-width: 100%;
    margin: 0 auto;
    padding: 1rem;
  }

  .custom-accordion {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .accordion-item {
    border: 1px solid #e0e0e0;
    border-radius: 4px;
    background: white;
    
    /* Stabilere Layout-Behandlung */
    overflow: hidden;
    position: relative;
  }

  .accordion-header {
    width: 100%;
    padding: 1rem;
    background: #f4f4f4;
    border: none;
    border-radius: 4px 4px 0 0;
    cursor: pointer;
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 1rem;
    font-weight: 500;
    transition: background-color 0.2s ease;
    
    &:hover {
      background: #e8e8e8;
    }
    
    &.open {
      background: #d0e2ff;
      color: #0043ce;
      border-radius: 4px 4px 0 0;
    }
  }

  .accordion-title {
    flex: 1;
    text-align: left;
  }

  .accordion-icon {
    font-size: 1.2rem;
    font-weight: bold;
    min-width: 20px;
    text-align: center;
  }

  .accordion-content {
    padding: 1rem;
    border-top: 1px solid #e0e0e0;
    
    /* Stable layout ohne Transitions */
    contain: layout style;
    
    /* Form-Styling übernehmen */
    :global(.bx--form-item) {
      margin-top: 0.5rem;
    }

    :global(.bx--label) {
      margin-bottom: 0.1rem;
    }

    :global(.bx--toggle-input__label .bx--toggle__switch) {
      margin-top: 0.1rem !important;
    }
  }
</style>