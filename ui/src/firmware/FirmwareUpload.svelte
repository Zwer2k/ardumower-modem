<script lang="ts">
  import {
    ComposedModal,
    ModalHeader,
    ModalFooter,
    ModalBody,
    ProgressBar,
    FileUploaderButton,
  } from "carbon-components-svelte";
  import type { Readable } from "svelte/store";
  import { FirmwareFlashStatus, FirmwareUploader, FirmwareUploadStatus, FirmwareUploadType } from "./service";

  export let open: boolean = false;
  export let uploadType: FirmwareUploadType = FirmwareUploadType.modem;

  let ref: null | HTMLInputElement;

  let fileSize = 0;

  let uploader = new FirmwareUploader();

  function uploadChange(e: CustomEvent<ReadonlyArray<File>>) {
    if (!(ref && ref.files && ref.files.length > 0)) {
      uploader.file = null;
      return;
    }
    const file = ref.files[0];
    uploader.file = file;
    fileSize = file.size;

    if (file !== null) uploader.upload(uploadType);
  }

  async function primary() {
    open = false;
  }

  let flashProgress: number | null = null;
  let flashStatus: FirmwareFlashStatus | null = null;
  let host = location.host;
  let ws = new WebSocket("ws://" + host + "/ws");

  ws.onopen = (event) => {
    console.log('WebSocket verbunden', event);
    // Optional: Sende eine erste Nachricht an den Server, um den Fortschritt anzufordern
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (data && typeof data.progress === 'number') {
        flashProgress = data.progress;
        if (flashProgress != null && flashProgress >= 100) {
          ws.close();
        }
      }
    } catch (error) {
      console.error('Fehler beim Parsen der WebSocket-Nachricht:', error);
    }
  };

  ws.onclose = (event) => {
    console.log('WebSocket getrennt', event);
    // Optional: Wiederverbindungslogik hier implementieren
  };

  ws.onerror = (error) => {
    console.error('WebSocket Fehler:', error);
  };

  function close() {
    if ($uploaderStatus === FirmwareUploadStatus.success)
      document.location.reload()
    ws.close();
  }

  let uploaderStatus: Readable<FirmwareUploadStatus> = uploader.status;
  let uploaderProgress: Readable<number> = uploader.progress;
</script>

<ComposedModal on:click:button--primary={primary} bind:open on:close={close}>
  <ModalHeader title="Upload modem firmware" />
  <ModalBody hasForm={true}>
    <ProgressBar
      value={$uploaderProgress}
      max={100}
      status={$uploaderStatus == FirmwareUploadStatus.success ? 'finished' : $uploaderStatus == FirmwareUploadStatus.error ? 'error' : undefined }
    />
    {#if flashProgress != null}
      <ProgressBar
        value={flashProgress}
        max={100}
        status={flashStatus == FirmwareUploadStatus.success ? 'finished' : flashStatus == FirmwareUploadStatus.error ? 'error' : undefined }
      />
    {/if}
    <p>Select the firmware update file on your computer.</p>
    <p>
      You can download the latest firmware updates on the <a
        href="https://github.com/timotto/ardumower-modem/releases"
        target="_blank">GitHub Releases</a
      > page.
    </p>
    <FileUploaderButton
      bind:ref
      on:change={uploadChange}
      disabled={fileSize > 0}
      accept={[".bin"]}
      labelText="Select..."
    />
    {#if $uploaderStatus >= FirmwareUploadStatus.fileSelected}
      <p>Size: {fileSize} bytes</p>
    {/if}

    {#if $uploaderStatus === FirmwareUploadStatus.uploading}
      <p>Uploading the firmware update...</p>
    {/if}
    {#if $uploaderStatus === FirmwareUploadStatus.expectReboot}
      <p>The firmware has been uploaded successfully.</p>
      <p>Waiting for the {uploadType} to restart...</p>
    {/if}

    {#if $uploaderStatus === FirmwareUploadStatus.error}
      <p>The update for the {uploadType} failed!</p>
      <p>The error message is <i>{uploader.error}</i></p>
    {/if}

    {#if $uploaderStatus === FirmwareUploadStatus.success}
      <p>The firmware update has been installed successfully.</p>
    {/if}
  </ModalBody>
  <ModalFooter
    secondaryButtonText="Cancel"
    primaryButtonText="Close"
    primaryButtonDisabled={$uploaderStatus !== FirmwareUploadStatus.success}
  />
</ComposedModal>
