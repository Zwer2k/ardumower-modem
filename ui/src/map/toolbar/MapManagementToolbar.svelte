<script lang="ts">
  import { Button, Row, Column } from "carbon-components-svelte";
  import IconSave from "carbon-icons-svelte/lib/Save.svelte";
  import IconUndo from "carbon-icons-svelte/lib/Undo.svelte";
  import IconPen from "carbon-icons-svelte/lib/Pen.svelte";
  import IconTrashCan from "carbon-icons-svelte/lib/TrashCan.svelte";
  import IconStar from "carbon-icons-svelte/lib/Star.svelte";
  import IconNew from "carbon-icons-svelte/lib/DocumentAdd.svelte";
  import type { MapWorkflow } from "../workflow/map-workflow-store";

  export let workflow: MapWorkflow;
  export let canSave: boolean;
  export let canRevert: boolean;
  export let canRename: boolean;
  export let effectiveMapId: string;
  export let pendingName: string;
  export let effectiveMapName: string;
  export let onSaveMap: () => void;
  export let onDiscardMap: () => void;
  export let startRename: () => void;
  export let onDeleteMap: () => void;
  export let onSetDefaultMap: () => void;
  export let onNewMap: () => void;
  export let workflowBusy: boolean;
</script>

<Row class="map-mgmt-row">
  <Column style="flex-shrink: 0;">
    <div class="toolbar-btn-row">
      <Button
        kind="primary"
        size="small"
        disabled={!canSave}
        on:click={onSaveMap}
        icon={IconSave}
        iconDescription="Save map"
      >
        <span class="btn-label">Save</span>
      </Button>
      <Button
        kind="tertiary"
        size="small"
        disabled={!canRevert}
        on:click={onDiscardMap}
        icon={IconUndo}
        iconDescription="Revert changes"
      >
        <span class="btn-label">Revert</span>
      </Button>
      <Button
        kind="secondary"
        size="small"
        disabled={!canRename || workflow.renameMode || workflowBusy}
        icon={IconPen}
        iconDescription="Rename map"
        on:click={startRename}
      >
        <span class="btn-label">Rename</span>
      </Button>
      <Button
        kind="danger"
        size="small"
        disabled={!effectiveMapId || workflow.renameMode || workflowBusy}
        on:click={onDeleteMap}
        icon={IconTrashCan}
        iconDescription="Delete map"
      >
        <span class="btn-label">Del</span>
      </Button>
      <Button
        kind="tertiary"
        size="small"
        disabled={!effectiveMapId || workflow.renameMode || workflowBusy}
        on:click={onSetDefaultMap}
        icon={IconStar}
        iconDescription="Set as default map"
      >
        <span class="btn-label">Default</span>
      </Button>
      <Button
        kind="secondary"
        size="small"
        disabled={workflow.renameMode || workflowBusy}
        on:click={onNewMap}
        icon={IconNew}
        iconDescription="New map"
      >
        <span class="btn-label">New</span>
      </Button>
      {#if pendingName && pendingName !== effectiveMapName}
        <span class="pending-map-name" title="Neuer Name, noch nicht gespeichert">→ {pendingName}</span>
      {/if}
    </div>
  </Column>
</Row>

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
  .pending-map-name {
    margin-left: 0.5rem;
    padding: 0.25rem 0.5rem;
    background: #fff3e0;
    color: #b06000;
    border: 1px solid #ffcc80;
    border-radius: 4px;
    font-size: 0.85em;
    font-weight: 600;
    white-space: nowrap;
    max-width: 200px;
    overflow: hidden;
    text-overflow: ellipsis;
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
