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
  import { onDestroy } from "svelte";
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

    if (file !== null) {
      uploader.upload(uploadType);
    }
  }

  async function primary() {
    open = false;
  }

  let flashProgress: number | null = null;
  let flashStatus: FirmwareFlashStatus | null = null;
  let ws: WebSocket | null = null;

  function initWebSocket() {
    console.log('WebSocket initialisieren');
    if (ws && ws.readyState === WebSocket.OPEN) {
      return; // Already connected
    }
    
    const host = location.host;
    ws = new WebSocket("ws://" + host + "/ws");

    ws.onopen = (event) => {
      console.log('WebSocket verbunden', event);
      flashStatus = FirmwareFlashStatus.clear; // Set to clear instead of null to indicate flashing started
      // Don't reset flashProgress here - let it be set by incoming messages
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        console.log('WebSocket Nachricht empfangen:', data);
        console.log('Upload Type:', uploadType);
        console.log('Data structure:', {
          hasProgress: typeof data.progress === 'number',
          hasStatus: !!data.status,
          hasStatusProgress: data.status && typeof data.status.progress === 'number',
          hasDataProgress: data.data && typeof data.data.progress === 'number',
          dataContent: data.data
        });
        
        // For mower uploads: check multiple possible formats
        if (uploadType === FirmwareUploadType.mower) {
          let progress = null;
          
          // Direct format: {"progress": 85}
          if (typeof data.progress === 'number') {
            progress = data.progress;
          }
          // Nested in data: {"type": 5, "data": {"progress": 85}}
          else if (data.data && typeof data.data.progress === 'number') {
            progress = data.data.progress;
          }
          
          if (progress !== null) {
            console.log('Mower progress update:', progress);
            flashProgress = progress;
            
            if (flashProgress >= 100) {
              flashStatus = FirmwareFlashStatus.success;
              closeWebSocket();
            } else if (flashProgress > 0) {
              flashStatus = FirmwareFlashStatus.clear; // In Progress
            }
          }
        }
        // For modem uploads: nested format {"status": {"progress": 85}}
        else if (uploadType === FirmwareUploadType.modem && data && data.status && typeof data.status.progress === 'number') {
          console.log('Modem progress update:', data.status.progress);
          flashProgress = data.status.progress;
          
          if (flashProgress >= 100) {
            flashStatus = FirmwareFlashStatus.success;
            closeWebSocket();
          } else if (flashProgress > 0) {
            flashStatus = FirmwareFlashStatus.clear; // In Progress
          }
        }
        // Fehler-Status prüfen
        else if (data && data.error) {
          console.error('Flash Fehler:', data.error);
          flashStatus = FirmwareFlashStatus.error;
          closeWebSocket();
        } else {
          console.log('Unbekanntes WebSocket-Nachrichtenformat oder Upload-Typ');
        }
      } catch (error) {
        console.error('Fehler beim Parsen der WebSocket-Nachricht:', error);
        flashStatus = FirmwareFlashStatus.error;
      }
    };

    ws.onclose = (event) => {
      console.log('WebSocket getrennt', event);
      ws = null;
    };

    ws.onerror = (error) => {
      console.error('WebSocket Fehler:', error);
      flashStatus = FirmwareFlashStatus.error;
    };
  }

  function closeWebSocket() {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.close();
    }
    ws = null;
  }

  function close() {
    const isSuccess = ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) || 
                     (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower);
    
    if (isSuccess)
      document.location.reload()
    closeWebSocket();
    // Reset flash progress und status
    flashProgress = null;
    flashStatus = null;
  }

  let uploaderStatus: Readable<FirmwareUploadStatus> = uploader.status;
  let uploaderProgress: Readable<number> = uploader.progress;

  // Reaktiv: WebSocket starten je nach Upload-Typ
  $: if (uploadType === FirmwareUploadType.modem && $uploaderStatus === FirmwareUploadStatus.expectReboot) {
    initWebSocket();
  } else if (uploadType === FirmwareUploadType.mower && $uploaderStatus === FirmwareUploadStatus.success) {
    initWebSocket();
  }

  onDestroy(() => {
    closeWebSocket();
  });
</script>

<style>
  :global(.progress-bar-container .bx--progress-bar) {
    width: 100% !important;
  }
  
  :global(.progress-bar-container .bx--progress-bar__track) {
    width: 100% !important;
  }
</style>

<ComposedModal on:click:button--primary={primary} bind:open on:close={close}>
  <ModalHeader title="Upload modem firmware" />
  <ModalBody hasForm={true}>
    <div style="width: 100%;">
      {#if $uploaderStatus >= FirmwareUploadStatus.fileSelected && $uploaderStatus < FirmwareUploadStatus.success && $uploaderStatus !== FirmwareUploadStatus.expectReboot}
        <div class="progress-bar-container" style="width: 100%; margin-bottom: 1rem;">
          <ProgressBar
            value={$uploaderProgress}
            max={100}
            status={$uploaderStatus == FirmwareUploadStatus.success ? 'finished' : $uploaderStatus == FirmwareUploadStatus.error ? 'error' : undefined }
            helperText="Upload progress"
          />
        </div>
      {/if}
      {#if flashProgress != null || (uploadType === FirmwareUploadType.mower && $uploaderStatus === FirmwareUploadStatus.success)}
        <div class="progress-bar-container" style="width: 100%; margin-bottom: 1rem;">
          <ProgressBar
            value={flashProgress != null ? Math.min(Math.max(flashProgress, 0), 100) : 0}
            max={100}
            status={flashStatus == FirmwareFlashStatus.success ? 'finished' : flashStatus == FirmwareFlashStatus.error ? 'error' : undefined }
            helperText="Firmware wird geflasht... ({flashProgress ?? 0}%)"
          />
        </div>
      {/if}
    </div>
    {#if $uploaderStatus < FirmwareUploadStatus.uploading}
      <p>Select the firmware update file on your computer.</p>
      <p>
        You can download the latest firmware updates on the <a
          href="https://github.com/timotto/ardumower-modem/releases"
          target="_blank">GitHub Releases</a
        > page.
      </p>
    {/if}
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
    {#if $uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.mower && flashStatus !== FirmwareFlashStatus.success}
      <p>The firmware has been uploaded successfully.</p>
      <p>Flashing {uploadType} firmware in progress...</p>
    {/if}
    
    {#if $uploaderStatus === FirmwareUploadStatus.expectReboot && uploadType === FirmwareUploadType.modem}
      <p>The firmware has been uploaded successfully.</p>
      <p>Waiting for the {uploadType} to restart...</p>
      {#if flashProgress != null && flashProgress > 0}
        <p>Flashing firmware in progress...</p>
      {/if}
    {/if}

    {#if $uploaderStatus === FirmwareUploadStatus.error}
      <p>The update for the {uploadType} failed!</p>
      <p>The error message is <i>{uploader.error}</i></p>
    {/if}

    {#if ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) || 
         (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower)}
      <p>The firmware update has been installed successfully.</p>
    {/if}
  </ModalBody>
  <ModalFooter
    secondaryButtonText="Cancel"
    primaryButtonText="Close"
    primaryButtonDisabled={!(
      ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) || 
      (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower)
    )}
  />
</ComposedModal>
