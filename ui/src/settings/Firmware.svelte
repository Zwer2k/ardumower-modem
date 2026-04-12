<script lang="ts">
  import {
    Button,
    StructuredList,
    StructuredListBody,
    StructuredListRow,
    StructuredListCell,
    Dropdown
  } from "carbon-components-svelte";
  import IconUpload from "carbon-icons-svelte/lib/CloudUpload.svelte";
  import {InfoStore} from "../stores/info"
  import { toastStore } from '../stores/toast';
  import { createEventDispatcher } from 'svelte';

  const dispatch = createEventDispatcher<{ uploadFirmware: void }>();

  let showRestartMenu = false;

  function uploadFw() {
    dispatch('uploadFirmware');
  }

  type RestartOption = { id: string; label: string; endpoint: string; text: string };
  let restartOptions: RestartOption[] = [
    { id: 'modem', label: 'Restart modem', endpoint: '/api/modem/reboot', text: 'Restart modem' },
    { id: 'mower', label: 'Restart mower', endpoint: '/api/mower/reboot', text: 'Restart mower' }
  ];
  let selectedRestartId: string | null = null;

  let selectedRestart: RestartOption | null = null;

  // Optional: close menu on outside click
  function handleClickOutside(event: MouseEvent) {
    if (!(event.target as HTMLElement).closest('.custom-dropdown-menu')) {
      showRestartMenu = false;
    }
  }

  $: {
    if (showRestartMenu) {
      window.addEventListener('mousedown', handleClickOutside);
    } else {
      window.removeEventListener('mousedown', handleClickOutside);
    }
  }

  async function restartDevice(option: RestartOption | null) {
    if (!option) return;
    try {
      const res = await fetch(option.endpoint, { method: 'POST' });
      if (!res.ok) {
        throw new Error('Restart request failed');
      }
      toastStore.set({ msg: `${option.label} command sent.`, type: 'success' });
    } catch (e) {
      let msg = 'unknown error';
      if (e instanceof Error) {
        msg = e.message;
      } else if (typeof e === 'string') {
        msg = e;
      }
      toastStore.set({ msg: `${option.label} failed: ` + msg, type: 'error' });
    }
  }
</script>

<div class="firmware-settings">
  {#if $InfoStore}
    <StructuredList>
      <StructuredListBody>
        <StructuredListRow>
          <StructuredListCell>Tag</StructuredListCell>
          <StructuredListCell>{$InfoStore.git_tag}</StructuredListCell>
        </StructuredListRow>
        <StructuredListRow>
          <StructuredListCell>Timestamp</StructuredListCell>
          <StructuredListCell>{$InfoStore.git_time}</StructuredListCell>
        </StructuredListRow>
        <StructuredListRow>
          <StructuredListCell>Source Version</StructuredListCell>
          <StructuredListCell>{$InfoStore.git_hash}</StructuredListCell>
        </StructuredListRow>
      </StructuredListBody>
    </StructuredList>
  {/if}
  <div style="display: flex; flex-direction: row; align-items: center; gap: 0.25rem; margin-top: 1rem; position: relative;">
    <Button on:click={uploadFw} icon={IconUpload}>Upload firmware</Button>
    <div style="position: relative;">
      <Button kind="primary" on:click={() => showRestartMenu = !showRestartMenu} aria-haspopup="true" aria-expanded={showRestartMenu} style="min-width: 140px; position: relative; padding-right: 2.2rem;">
        <span style="display: block; text-align: left;">Restart...</span>
        <span style="position: absolute; right: 0.75rem; top: 50%; transform: translateY(-50%); display: flex; align-items: center;">
          <svg width="20" height="20" viewBox="0 0 32 32" fill="none"><path d="M16 22L6 12 7.41 10.59 16 19.17 24.59 10.59 26 12 16 22z" fill="currentColor"/></svg>
        </span>
      </Button>
      {#if showRestartMenu}
        <div class="custom-dropdown-menu" style="position: absolute; bottom: 110%; left: 0; z-index: 100; background: white; border: 1px solid #e0e0e0; border-radius: 0.25rem; box-shadow: 0 -2px 8px rgba(0,0,0,0.08); min-width: 140px;">
          {#each restartOptions as option}
            <Button kind="primary" style="width: 100%; border-radius: 0; justify-content: flex-start; text-align: left; padding-left: 1rem; padding-right: 1rem; min-width: 140px;" on:click={() => { showRestartMenu = false; restartDevice(option); }}>
              {option.label}
            </Button>
          {/each}
        </div>
      {/if}
    </div>
  </div>
</div>

<style lang="scss">
  .firmware-settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }
</style>
