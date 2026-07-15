<script lang="ts">
  import { Button, Row, Column } from "carbon-components-svelte";
  import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";
  import IconTrashCan from "carbon-icons-svelte/lib/TrashCan.svelte";
  import IconMagicWand from "carbon-icons-svelte/lib/MagicWand.svelte";
  import { socketService } from "../../stores/socket";

  export let busy: boolean;
  export let onOpenMowSettings: () => void;
</script>

<Row class="map-calc-row">
  <Column>
    <div class="toolbar-btn-row">
      <Button
        kind="secondary"
        size="small"
        icon={IconSettings}
        iconDescription="Mow settings"
        on:click={onOpenMowSettings}
      />
      <Button
        kind="danger"
        size="small"
        disabled={busy}
        icon={IconTrashCan}
        iconDescription="Clear waypoints"
        on:click={() => socketService.sendClearWaypoints()}
      >
        <span class="btn-label">Clear WP</span>
      </Button>
      <Button
        kind="secondary"
        size="small"
        disabled={busy}
        icon={IconMagicWand}
        iconDescription="Calculate waypoints"
        on:click={() => socketService.sendCalculateWaypoints()}
      >
        <span class="btn-label">Calculate</span>
      </Button>
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
