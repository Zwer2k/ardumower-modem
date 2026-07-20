<script lang="ts">
  import {
    Button,
    Modal,
    TextArea,
    InlineNotification,
  } from "carbon-components-svelte";
  import IconCopy from "carbon-icons-svelte/lib/Copy.svelte";
  import IconDocumentDownload from "carbon-icons-svelte/lib/DocumentDownload.svelte";
  import {
    exportCassandraMap,
    importCassandraMap,
    isValidCassandraMap,
  } from "./core/cassandra-map";
  import type { Map } from "../model";
  import { SaveSuccess } from "../stores/success";

  export let open = false;
  export let map: Map;
  export let rotation: number = 0;
  export let onImport: (map: Map, rotation: number) => void;

  let mode: "export" | "import" = "export";
  let jsonText = "";
  let importError = "";
  let exportError = "";

  $: if (open) {
    mode = "export";
    jsonText = exportCassandraMap(map, rotation);
    importError = "";
    exportError = "";
  }

  function switchMode(newMode: "export" | "import") {
    mode = newMode;
    importError = "";
    exportError = "";
    if (mode === "export") {
      jsonText = exportCassandraMap(map, rotation);
    } else {
      jsonText = "";
    }
  }

  function copyWithFallback(text: string): boolean {
    if (typeof navigator !== "undefined" && navigator.clipboard) {
      void navigator.clipboard.writeText(text);
      return true;
    }
    if (typeof document === "undefined") {
      return false;
    }
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.setAttribute("readonly", "");
    ta.style.position = "fixed";
    ta.style.left = "-9999px";
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    let ok = false;
    try {
      ok = document.execCommand("copy");
    } catch {
      ok = false;
    }
    document.body.removeChild(ta);
    return ok;
  }

  function handleCopy() {
    if (copyWithFallback(jsonText)) {
      SaveSuccess.set({ action: "copy cassandra map", date: new Date() });
    } else {
      exportError = "Could not copy to clipboard. Please use Download or select and copy manually.";
    }
  }

  function handleDownload() {
    const blob = new Blob([jsonText], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "cassandra-map.json";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function handleImport() {
    const trimmed = jsonText.trim();
    if (!trimmed) {
      importError = "Paste JSON first.";
      return;
    }
    if (!isValidCassandraMap(trimmed)) {
      importError = "Invalid CassandRA map JSON: at least 3 perimeter points required.";
      return;
    }
    const result = importCassandraMap(trimmed);
    if (!result) {
      importError = "Failed to import map.";
      return;
    }
    onImport(result.map, result.rotation);
    SaveSuccess.set({ action: "import cassandra map", date: new Date() });
    open = false;
  }

  function handleClose() {
    open = false;
  }
</script>

<Modal
  bind:open
  modalHeading="CassandRA Map Import / Export"
  primaryButtonText={mode === "import" ? "Import" : "Close"}
  secondaryButtonText={mode === "import" ? "Close" : undefined}
  on:click:button--primary={mode === "import" ? handleImport : handleClose}
  on:click:button--secondary={mode === "import" ? handleClose : undefined}
  on:close={handleClose}
>
  <div class="mode-toggle">
    <Button
      kind={mode === "export" ? "primary" : "tertiary"}
      size="small"
      on:click={() => switchMode("export")}
    >
      Export
    </Button>
    <Button
      kind={mode === "import" ? "primary" : "tertiary"}
      size="small"
      on:click={() => switchMode("import")}
    >
      Import
    </Button>
  </div>

  {#if mode === "export"}
    <div class="export-actions">
      <Button kind="secondary" size="small" on:click={handleCopy} icon={IconCopy}>
        Copy to clipboard
      </Button>
      <Button kind="secondary" size="small" on:click={handleDownload} icon={IconDocumentDownload}>
        Download
      </Button>
    </div>
  {/if}

  {#if exportError}
    <InlineNotification
      kind="error"
      title="Export error"
      subtitle={exportError}
      hideCloseButton
    />
  {/if}

  {#if importError}
    <InlineNotification
      kind="error"
      title="Import error"
      subtitle={importError}
      hideCloseButton
    />
  {/if}

  <TextArea
    labelText={mode === "export" ? "CassandRA map JSON" : "Paste CassandRA map JSON"}
    placeholder={mode === "import" ? '{ "perimeter": [{"x":0,"y":0}, ...] }' : ""}
    bind:value={jsonText}
    rows={14}
    disabled={mode === "export"}
  />
</Modal>

<style>
  .mode-toggle {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1rem;
  }
  .export-actions {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1rem;
  }
</style>
