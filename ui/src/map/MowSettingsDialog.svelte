<script lang="ts">
  import { Button, Modal, NumberInput, Select, SelectItem, Toggle } from "carbon-components-svelte";
  import { mowSettingsStore } from "./mow-settings";
  import { socketService } from "../stores/socket";

  export let open = false;

  let pattern: number = 0;
  let width: number = 0.3;
  let angle: number = 0;
  let distanceToBorder: number = 0;
  let borderLaps: number = 0;
  let mowArea: boolean = true;
  let mowExclusionBorder: boolean = false;
  let mowBorderCcw: boolean = false;

  const patterns = [
    { id: 0, text: "Lines" },
    { id: 1, text: "Squares" },
    { id: 2, text: "Rings" },
  ];

  function roundWidth(v: number) {
    width = Math.round(v * 100) / 100;
  }

  function handleOpen() {
    const s = $mowSettingsStore;
    pattern = s.pattern;
    roundWidth(s.width);
    angle = s.angle;
    distanceToBorder = s.distanceToBorder;
    borderLaps = s.borderLaps;
    mowArea = s.mowArea;
    mowExclusionBorder = s.mowExclusionBorder;
    mowBorderCcw = s.mowBorderCcw;
  }

  function handleOk() {
    socketService.sendMowSettings({
      pattern,
      width,
      angle,
      distanceToBorder,
      borderLaps,
      mowArea,
      mowExclusionBorder,
      mowBorderCcw,
    });
    open = false;
  }

  function handleCancel() {
    open = false;
  }
</script>

<Modal
  bind:open
  on:open={handleOpen}
  title="Mow Settings"
  size="sm"
  id="mow-settings-modal"
  hasScrollingContent={true}
  primaryButtonText="Apply"
  secondaryButtonText="Cancel"
  on:click:button--primary={handleOk}
  on:click:button--secondary={handleCancel}
  on:submit={handleOk}
  on:close={handleCancel}
>
  <div class="mow-settings-form">
    <Select
      labelText="Pattern"
      selected={pattern}
      on:update={(e) => { pattern = e.detail; }}
    >
      {#each patterns as p}
        <SelectItem value={p.id} text={p.text} />
      {/each}
    </Select>

    <NumberInput
      label="Track width (m)"
      bind:value={width}
      min={0.01}
      max={1.0}
      step={0.01}
      on:change={() => roundWidth(width)}
    />

    <NumberInput
      label="Angle (°)"
      bind:value={angle}
      min={0}
      max={359}
      step={1}
    />

    <NumberInput
      label="Distance to border"
      bind:value={distanceToBorder}
      min={0}
      max={5}
      step={1}
    />

    <NumberInput
      label="Border laps"
      bind:value={borderLaps}
      min={0}
      max={6}
      step={1}
    />

    <Toggle
      labelText="Mow area"
      toggled={mowArea}
      on:toggle={(e) => { mowArea = e.detail.toggled; }}
    />

    <Toggle
      labelText="Mow exclusion border"
      toggled={mowExclusionBorder}
      on:toggle={(e) => { mowExclusionBorder = e.detail.toggled; }}
    />

    <Toggle
      labelText="Mow border CCW"
      toggled={mowBorderCcw}
      on:toggle={(e) => { mowBorderCcw = e.detail.toggled; }}
    />
  </div>
</Modal>

<style>
  .mow-settings-form {
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding: 0.5rem 0;
  }

  /* Make this dialog a flex column so the content area shrinks to fit between
     header and footer, scrolls internally, and never pushes the buttons out of
     the viewport. */
  :global(#mow-settings-modal .bx--modal-container) {
    display: flex !important;
    flex-direction: column !important;
    max-height: 90vh !important;
  }
  :global(#mow-settings-modal .bx--modal-content) {
    flex: 1 1 auto !important;
    min-height: 0 !important;
    overflow-y: auto !important;
    margin-bottom: 0 !important;
  }
  :global(#mow-settings-modal .bx--modal-content--overflow-indicator) {
    display: none !important;
  }
</style>
