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
  import FirmwareUpload from '../firmware/FirmwareUpload.svelte'

  let dialogFwOpen = false;

  function uploadFw() {
    dialogFwOpen = true;
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
</Group>
