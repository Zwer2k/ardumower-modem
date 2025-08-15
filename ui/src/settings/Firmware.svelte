<script lang="ts">
  import {
    Button,
    StructuredList,
    StructuredListBody,
    StructuredListRow,
    StructuredListCell,
  } from "carbon-components-svelte";
  import IconUpload from "carbon-icons-svelte/lib/CloudUpload.svelte";
  import Group from "./Group.svelte";
  import {InfoStore} from "../stores/info"
  import { toastStore } from '../stores/toast';
  import FirmwareUpload from '../firmware/FirmwareUpload.svelte'

  let dialogFwOpen = false;

  function uploadFw() {
    dialogFwOpen = true;
  }

  async function restartMower() {
    try {
      const res = await fetch('/api/mower/reboot', { method: 'POST' });
      if (!res.ok) {
        throw new Error('Restart request failed');
      }
      toastStore.set({ msg: 'Restart command sent to mower.', type: 'success' });
    } catch (e) {
      let msg = 'unknown error';
      if (e instanceof Error) {
        msg = e.message;
      } else if (typeof e === 'string') {
        msg = e;
      }
      toastStore.set({ msg: 'Restart failed: ' + msg, type: 'error' });
    }
  }
</script>

<FirmwareUpload bind:open={dialogFwOpen} />
<Group title="Firmware" open={true}>
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
  <Button on:click={uploadFw} icon={IconUpload}>Upload firmware</Button>
  <Button on:click={restartMower} style="margin-left: 0.5rem;">Restart mower</Button>
</Group>
