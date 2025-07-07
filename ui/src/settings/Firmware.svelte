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
    import { FirmwareUploadType } from "../firmware/service";

  let dialogModemFwOpen = false;
  let dialogMowerFwOpen = false;

  function uploadModemFw() {
    dialogModemFwOpen = true;
  }

  function uploadMowerFw() {
    dialogMowerFwOpen = true;
  }
</script>

<FirmwareUpload bind:open={dialogModemFwOpen} bind:uploadType={FirmwareUploadType.modem} />
<FirmwareUpload bind:open={dialogMowerFwOpen} bind:uploadType={FirmwareUploadType.mower} />
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
  <Button on:click={uploadModemFw} icon={IconUpload}>Upload modem firmware</Button>
  <Button on:click={uploadMowerFw} icon={IconUpload}>Upload mower firmware</Button>
</Group>
