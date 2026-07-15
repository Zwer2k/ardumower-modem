<script lang="ts">
  import {
    ComposedModal,
    ModalHeader,
    ModalFooter,
    ModalBody,
    ProgressBar,
    FileUploaderButton,
    Dropdown,
  } from "carbon-components-svelte";
  import type { Readable } from "svelte/store";
  import { onDestroy } from "svelte";
  import { FirmwareFlashStatus, FirmwareUploader, FirmwareUploadStatus, FirmwareUploadType } from "./service";

  export let open: boolean = false;

  let uploadType: FirmwareUploadType = FirmwareUploadType.modem;
  let ref: null | HTMLInputElement;

  const uploadTypeOptions = [
    { id: FirmwareUploadType.modem, text: "Modem Firmware" },
    { id: FirmwareUploadType.mower, text: "Mower Firmware" }
  ];

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

  function handleUploadTypeChange(e: CustomEvent<{ selectedId: FirmwareUploadType }>) {
    uploadType = e.detail.selectedId;
    // Reset upload state when changing type
    resetUploadState();
  }

  function resetUploadState() {
    uploader.file = null;
    fileSize = 0;
    flashProgress = null;
    flashStatus = null;
    flashError = null;
    stopReconnecting();
    stopWatchdog();
    closeWebSocket();
    if (ref) {
      ref.value = '';
    }
  }

  async function primary() {
    open = false;
  }

  let flashProgress: number | null = null;
  let flashStatus: FirmwareFlashStatus | null = null;
  let flashError: string | null = null;
  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let watchdogTimer: ReturnType<typeof setTimeout> | null = null;
  let watchdogStartedAt: number | null = null;

  const FLASH_WATCHDOG_MS = 5 * 60 * 1000; // 5 minutes max for a flash
  const RECONNECT_BASE_MS = 500;
  const RECONNECT_MAX_MS = 8000;
  let reconnectAttempt = 0;

  function stopReconnecting() {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    reconnectAttempt = 0;
  }

  function stopWatchdog() {
    if (watchdogTimer) {
      clearTimeout(watchdogTimer);
      watchdogTimer = null;
    }
    watchdogStartedAt = null;
  }

  function startWatchdog() {
    stopWatchdog();
    watchdogStartedAt = Date.now();
    watchdogTimer = setTimeout(() => {
      if (flashStatus !== FirmwareFlashStatus.success) {
        console.error('[FirmwareUpload] flash watchdog timeout');
        flashError = 'Zeitüberschreitung beim Flashen';
        flashStatus = FirmwareFlashStatus.error;
        closeWebSocket();
      }
    }, FLASH_WATCHDOG_MS);
  }

  function scheduleReconnect() {
    if (flashStatus === FirmwareFlashStatus.success || flashStatus === FirmwareFlashStatus.error) return;
    stopReconnecting();
    const delay = Math.min(RECONNECT_BASE_MS * Math.pow(2, reconnectAttempt), RECONNECT_MAX_MS);
    reconnectAttempt += 1;
    console.log('[FirmwareUpload] scheduling reconnect in', delay, 'ms (attempt', reconnectAttempt, ')');
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connectWebSocket();
    }, delay);
  }

  function connectWebSocket() {
    console.log('[FirmwareUpload] connectWebSocket');
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
      return;
    }
    closeWebSocket();

    const host = location.host;
    try {
      ws = new WebSocket('ws://' + host + '/ws');
    } catch (err) {
      console.error('[FirmwareUpload] WebSocket construction failed:', err);
      scheduleReconnect();
      return;
    }

    ws.onopen = (event) => {
      console.log('[FirmwareUpload] WebSocket open', event);
      reconnectAttempt = 0;
      if (flashStatus !== FirmwareFlashStatus.success) {
        flashStatus = FirmwareFlashStatus.clear;
      }
      if (watchdogStartedAt === null) {
        startWatchdog();
      }
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        console.log('[FirmwareUpload] WebSocket message:', data);

        let progress: number | null = null;

        if (uploadType === FirmwareUploadType.mower) {
          if (typeof data.progress === 'number') {
            progress = data.progress;
          } else if (data.data && typeof data.data.progress === 'number') {
            progress = data.data.progress;
          }
        } else if (uploadType === FirmwareUploadType.modem && data && data.status && typeof data.status.progress === 'number') {
          progress = data.status.progress;
        }

        if (progress !== null) {
          flashProgress = Math.max(0, Math.min(100, progress));
          if (flashProgress >= 100) {
            flashStatus = FirmwareFlashStatus.success;
            stopReconnecting();
            stopWatchdog();
            closeWebSocket();
          } else if (flashStatus !== FirmwareFlashStatus.success) {
            flashStatus = FirmwareFlashStatus.clear;
          }
          return;
        }

        if (data && data.error) {
          console.error('[FirmwareUpload] flash error from backend:', data.error);
          flashError = data.error;
          flashStatus = FirmwareFlashStatus.error;
          stopReconnecting();
          stopWatchdog();
          closeWebSocket();
        }
      } catch (error) {
        console.error('[FirmwareUpload] error parsing WebSocket message:', error);
      }
    };

    ws.onclose = (event) => {
      console.log('[FirmwareUpload] WebSocket close', event.code, event.reason);
      ws = null;
      if (flashStatus !== FirmwareFlashStatus.success && flashStatus !== FirmwareFlashStatus.error) {
        scheduleReconnect();
      }
    };

    ws.onerror = (error) => {
      console.error('[FirmwareUpload] WebSocket error:', error);
      ws = null;
      if (flashStatus !== FirmwareFlashStatus.success && flashStatus !== FirmwareFlashStatus.error) {
        scheduleReconnect();
      }
    };
  }

  function closeWebSocket() {
    stopReconnecting();
    if (ws) {
      if (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING) {
        try {
          ws.close();
        } catch (_) {
          // ignore
        }
      }
      ws = null;
    }
  }

  function close() {
    const isSuccess =
      ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) ||
      (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower);

    if (isSuccess) document.location.reload();
    resetUploadState();
  }

  let uploaderStatus: Readable<FirmwareUploadStatus> = uploader.status;
  let uploaderProgress: Readable<number> = uploader.progress;

  $: if (uploadType === FirmwareUploadType.modem && $uploaderStatus === FirmwareUploadStatus.expectReboot) {
    connectWebSocket();
  } else if (uploadType === FirmwareUploadType.mower && $uploaderStatus === FirmwareUploadStatus.success) {
    connectWebSocket();
  }

  $: if (uploadType === FirmwareUploadType.modem && $uploaderStatus === FirmwareUploadStatus.success) {
    flashProgress = 100;
    flashStatus = FirmwareFlashStatus.success;
    stopReconnecting();
    stopWatchdog();
    closeWebSocket();
  }

  onDestroy(() => {
    stopReconnecting();
    stopWatchdog();
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

  /* Fix for 100% progress bar width */
  :global(.progress-bar-container .bx--progress-bar--finished .bx--progress-bar__bar) {
    transform: scaleX(1) !important;
  }

  :global(.progress-bar-container .bx--progress-bar__bar[style*="100"]) {
    transform: scaleX(1) !important;
  }
  
  /* Ensure dropdown doesn't cause scrollbars */
  :global(.bx--modal-container) {
    overflow: visible !important;
  }
  
  :global(.bx--modal-content) {
    overflow: visible !important;
  }
  
  :global(.bx--dropdown__wrapper) {
    z-index: 9999;
  }
</style>

<ComposedModal on:click:button--primary={primary} bind:open on:close={close}>
  <ModalHeader title="Upload Firmware" />
  <ModalBody hasForm={true}>
    {#if $uploaderStatus < FirmwareUploadStatus.fileSelected}
      <div style="width: 100%; margin-bottom: 1rem; position: relative; z-index: 1000;">
        <Dropdown
          titleText="Select firmware type"
          items={uploadTypeOptions}
          selectedId={uploadType}
          on:select={handleUploadTypeChange}
          direction="bottom"
        />
      </div>
    {/if}
    <div style="width: 100%;">
      {#if $uploaderStatus >= FirmwareUploadStatus.fileSelected}
        <div class="progress-bar-container" style="width: 100%; margin-bottom: 1rem;">
          <ProgressBar
            value={$uploaderProgress}
            max={100}
            status={
              $uploaderStatus == FirmwareUploadStatus.error ? 'error' :
              (uploadType === FirmwareUploadType.modem && flashStatus === FirmwareFlashStatus.success) ? 'finished' :
              (uploadType === FirmwareUploadType.mower && $uploaderStatus === FirmwareUploadStatus.success) ? 'finished' :
              (uploadType === FirmwareUploadType.modem && $uploaderStatus === FirmwareUploadStatus.success) ? 'finished' :
              undefined
            }
            helperText="Upload progress"
          />
        </div>
      {/if}
      {#if flashProgress != null || (uploadType === FirmwareUploadType.mower && $uploaderStatus === FirmwareUploadStatus.success)}
        <div class="progress-bar-container" style="width: 100%; margin-bottom: 1rem;">
          <ProgressBar
            value={flashStatus === FirmwareFlashStatus.success ? 100 : (flashProgress != null ? Math.round(Math.min(Math.max(flashProgress, 0), 100)) : 0)}
            max={100}
            status={flashStatus == FirmwareFlashStatus.success ? 'finished' : flashStatus == FirmwareFlashStatus.error ? 'error' : undefined }
            helperText="Firmware wird geflasht... ({flashStatus === FirmwareFlashStatus.success ? 100 : (flashProgress != null ? Math.round(flashProgress) : 0)}%)"
          />
        </div>
      {/if}
    </div>
    {#if $uploaderStatus < FirmwareUploadStatus.uploading}
      <p>Select the firmware update file on your computer.</p>
      {#if uploadType === FirmwareUploadType.modem}
        <p>
          You can download the latest firmware updates on the <a
            href="https://github.com/timotto/ardumower-modem/releases"
            target="_blank">GitHub Releases</a
          > page.
        </p>
      {/if}
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
      <p>Uploading the {uploadType} firmware update...</p>
    {/if}
    {#if $uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.mower && flashStatus !== FirmwareFlashStatus.success}
      <p>The {uploadType} firmware has been uploaded successfully.</p>
      <p>Flashing {uploadType} firmware in progress... {flashError ? `(${flashError})` : ''}</p>
    {/if}
    
    {#if $uploaderStatus === FirmwareUploadStatus.expectReboot && uploadType === FirmwareUploadType.modem}
      <p>The {uploadType} firmware has been uploaded successfully.</p>
      <p>Waiting for the {uploadType} to restart... {flashError ? `(${flashError})` : ''}</p>
      {#if flashProgress != null && flashProgress > 0}
        <p>Flashing firmware in progress...</p>
      {/if}
    {/if}

    {#if $uploaderStatus === FirmwareUploadStatus.error}
      <p>The {uploadType} firmware update failed!</p>
      <p>The error message is <i>{uploader.error}</i></p>
    {/if}

    {#if flashStatus === FirmwareFlashStatus.error}
      <p>The {uploadType} firmware flash failed!</p>
      {#if flashError}
        <p>The error message is <i>{flashError}</i></p>
      {/if}
    {/if}

    {#if ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) || 
         (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower)}
      <p>The {uploadType} firmware update has been installed successfully.</p>
    {/if}
  </ModalBody>
  <ModalFooter
    secondaryButtonText="Cancel"
    primaryButtonText="Close"
    primaryButtonDisabled={!(
      ($uploaderStatus === FirmwareUploadStatus.success && uploadType === FirmwareUploadType.modem) || 
      (flashStatus === FirmwareFlashStatus.success && uploadType === FirmwareUploadType.mower) ||
      (flashStatus === FirmwareFlashStatus.error)
    )}
  />
</ComposedModal>
