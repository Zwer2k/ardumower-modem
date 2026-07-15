<script lang="ts">
  import { Button } from "carbon-components-svelte";
  import IconUpload from "carbon-icons-svelte/lib/Upload.svelte";
  import IconEdit from "carbon-icons-svelte/lib/Edit.svelte";
  import IconTools from "carbon-icons-svelte/lib/Tools.svelte";
  import IconMagicWand from "carbon-icons-svelte/lib/MagicWand.svelte";
  import IconCheckmark from "carbon-icons-svelte/lib/Checkmark.svelte";
  import IconClose from "carbon-icons-svelte/lib/Close.svelte";

  export let workflowBusy: boolean;
  export let busy: boolean;
  export let renameMode: boolean;
  export let showManage: boolean;
  export let edit: boolean;
  export let showCalculate: boolean;
  export let canConfirmRename: boolean;
  export let onUpload: () => void;
  export let onToggleManage: () => void;
  export let onToggleEdit: () => void;
  export let onToggleCalculate: () => void;
  export let onConfirmRename: () => void;
  export let onCancelRename: () => void;
</script>

<div class="toolbar-btn-row">
  {#if renameMode}
    <Button
      kind="primary"
      size="small"
      disabled={!canConfirmRename}
      on:click={onConfirmRename}
      icon={IconCheckmark}
      iconDescription="Confirm rename"
    >
      <span class="btn-label">OK</span>
    </Button>
    <Button
      kind="secondary"
      size="small"
      on:click={onCancelRename}
      icon={IconClose}
      iconDescription="Cancel rename"
    >
      <span class="btn-label">Cancel</span>
    </Button>
  {:else}
    <Button
      kind="secondary"
      size="small"
      disabled={busy || workflowBusy}
      icon={IconUpload}
      iconDescription="Upload map to mower"
      on:click={onUpload}
    />
    <Button
      kind={showManage ? "primary" : "secondary"}
      size="small"
      icon={IconTools}
      iconDescription="Manage maps"
      on:click={onToggleManage}
    />
    <Button
      kind={edit ? "primary" : "secondary"}
      size="small"
      icon={IconEdit}
      iconDescription="Edit map"
      on:click={onToggleEdit}
    />
    <Button
      kind={showCalculate ? "primary" : "secondary"}
      size="small"
      icon={IconMagicWand}
      iconDescription="Calculate waypoints"
      on:click={onToggleCalculate}
    />
  {/if}
</div>

<style>
  .toolbar-btn-row {
    display: flex;
    flex: 0 0 auto;
    flex-wrap: nowrap;
    gap: 0.25rem;
  }
  .btn-label {
    margin-left: 0.25rem;
  }
  .toolbar-btn-row :global(.bx--btn) {
    padding-left: 0.3125rem !important;
    padding-right: 0.3125rem !important;
  }
  .toolbar-btn-row :global(.bx--btn .bx--btn__icon) {
    position: static;
    margin-left: 0.5rem;
    margin-right: 0.25rem;
  }
  @media (max-width: 800px) {
    .btn-label {
      display: none;
    }
    .toolbar-btn-row :global(.bx--btn) {
      width: 2rem;
      padding: 0 !important;
      justify-content: center;
    }
    .toolbar-btn-row :global(.bx--btn .bx--btn__icon) {
      position: static;
      margin: 0;
    }
  }
</style>
