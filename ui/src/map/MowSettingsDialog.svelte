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

  $: {
    if (open) {
      const s = $mowSettingsStore;
      pattern = s.pattern;
      width = s.width;
      angle = s.angle;
      distanceToBorder = s.distanceToBorder;
      borderLaps = s.borderLaps;
      mowArea = s.mowArea;
      mowExclusionBorder = s.mowExclusionBorder;
      mowBorderCcw = s.mowBorderCcw;
    }
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
  title="Mow Settings"
  size="sm"
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
      bind:value={pattern}
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
      format={(v: number) => v.toFixed(2)}
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
      bind:toggled={mowArea}
    />

    <Toggle
      labelText="Mow exclusion border"
      bind:toggled={mowExclusionBorder}
    />

    <Toggle
      labelText="Mow border CCW"
      bind:toggled={mowBorderCcw}
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
</style>
