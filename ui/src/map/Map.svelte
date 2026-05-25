<script lang="ts">
  import {
    Button,
    ButtonSet,
    Checkbox,
    ComboBox,
    Slider,
    Row,
    Column,
    Grid,
  } from "carbon-components-svelte";
  import Canvas from "./Canvas.svelte";
  import Exclusion from "./Exclusion.svelte";
  import { pointsToEdges } from "./geometry";
  import type { Point, Edge } from "./model";
  import Perimeter from "./Perimeter.svelte";
  import Waypoints from "./Waypoints.svelte";
  import Dockpoints from "./Dockpoints.svelte";
  import MowerPosition from "./MowerPosition.svelte";
  import { MapStore } from "./service";
  import { socketStore } from "../stores/socket";
  import * as localGotoService from "../localGotoService";
  import { onMount, onDestroy } from "svelte";

  interface EditItem {
    id: string;
    text: string;
  }

  let edit = false;
  let selectedId: string | null = null;
  let editItemId: string | null = null;
  let editItems: EditItem[] = [];

  let editPoint = false;
  let editEdge = false;

  const pointsToEditItem =
    (idPrefix: string, textPrefix: string) => (p: Point, index: number) => ({
      id: idPrefix + index,
      text: textPrefix + index,
    });

  const edgesToEditItem =
    (idPrefix: string, textPrefix: string) =>
    (e: Edge, index: number): EditItem => ({
      id: idPrefix + index,
      text: textPrefix + index,
    });


  $: editItems = $MapStore && $MapStore.map
    ? [
        $MapStore.map.perimeter.points.map(
          pointsToEditItem("map-0-perimeter-point-", "Perimeter point #")
        ),
        pointsToEdges($MapStore.map.perimeter.points).map(
          edgesToEditItem("map-0-perimeter-edge-", "Perimeter edge #")
        ),
        ...$MapStore.map.exclusions.map((e, i) => [
          ...e.points.map(
            pointsToEditItem(
              `map-0-exclusion-${i}-point-`,
              `Exclution #${i} point #`
            )
          ),
          ...pointsToEdges(e.points).map(
            edgesToEditItem(`map-0-exclusion-${i}-edge-`, `Exclusion #${i} edge #`)
          ),
        ]),
      ].reduce((a, b) => [...a, ...b], [])
    : [];

  function shouldFilterItem(item: EditItem, value: string) {
    if (!value) return true;
    return item.text.toLowerCase().includes(value.toLowerCase());
  }

  let uiEditId: null | string = null;

  function updateButtonAvailability() {
    editPoint = edit && !!editItemId && editItemId.indexOf("-point-") !== -1;
    editEdge = edit && !!editItemId && editItemId.indexOf("-edge-") !== -1;
  }

  function selectEditItem(e: any) {
    uiEditId = editItemId = e["detail"]["selectedId"];
    updateButtonAvailability();
  }

  function clearEditItem() {
    editItemId = null;
  }

  function onDeleteClick() {
    if (editItemId != null) {
      if (editPoint) {
        let index = parseInt(editItemId.replace(/.*-point-([0-9]+)/, "$1"));
        if (editItemId.indexOf("-perimeter-") !== -1) {
          $MapStore.map.perimeter.points.splice(index, 1);
          console.log("delete point", editItemId, index, $MapStore.map.perimeter.points);
        } else {
          let exclusion = parseInt(editItemId.replace(/.*-exclusion-([0-9]+).*/, "$1"));
          $MapStore.map.exclusions[exclusion].points.splice(index, 1);
          console.log("delete point", editItemId, index, $MapStore.map.perimeter.points);
        }
        $MapStore.map.perimeter = $MapStore.map.perimeter;
      }
    }
  }

  function omg(item: null | string) {
    if (item == uiEditId) return;

    if (item === null) return;

    let editItemIndex = editItems
      .map((item, index) => (item.id === editItemId ? index : -1))
      .reduce((a, b) => (a > b ? a : b), -1);

    selectedId = editItemIndex > -1 ? editItems[editItemIndex].id : null;
    updateButtonAvailability();
  }

  $: omg(editItemId);

  // ─── Goto ────────────────────────────────────────────────────────────────

  let targetSet = false;
  let isDriving = false;
  let targetDist = 0;
  let targetBearing = 0;

  let _unsubSocket: () => void;

  onMount(() => {
    _unsubSocket = socketStore.subscribe(($s) => {
      if ($s.state?.position) {
        localGotoService.setPosition($s.state.position);
      }
    });

    localGotoService.onUpdate((dist, bearing) => {
      targetDist = dist;
      targetBearing = bearing;
    });

    localGotoService.onArrive(() => {
      isDriving = false;
    });
  });

  onDestroy(() => {
    localGotoService.stop();
    _unsubSocket?.();
  });

  function onMapClick(event: CustomEvent<{ x: number; y: number }>) {
    if (edit) return;
    if (isDriving) return;
    const { x, y } = event.detail;
    localGotoService.setTarget(x, y);
    targetSet = true;
  }

  function startDrive() {
    if (!targetSet) return;
    if (localGotoService.start()) {
      isDriving = true;
    }
  }

  function stopDrive() {
    localGotoService.stop();
    isDriving = false;
  }

  function clearTarget() {
    localGotoService.clearTarget();
    targetSet = false;
    targetDist = 0;
    targetBearing = 0;
  }

  // Derived target position
  $: targetPos = localGotoService.getTarget();
  $: mowerPos = $socketStore.state?.position ?? null;
</script>

<div class="map-dashboard">
  <Grid class="map-toolbar">
    <Row>
      <Column sm={1} md={1} lg={1}>
        <Checkbox bind:checked={edit} labelText="Editor" />
      </Column>
      <Column>
        <ComboBox
          disabled={!edit}
          placeholder="Select item to edit"
          items={editItems}
          bind:selectedId
          on:select={selectEditItem}
          on:clear={clearEditItem}
          {shouldFilterItem}
        />
      </Column>
      <Column>
        <Button kind="tertiary" size="small" disabled={!editPoint}
          >Duplicate</Button
        >
        <Button kind="tertiary" size="small" disabled={!editEdge}>Split</Button>
        <Button kind="tertiary" size="small" disabled={!editEdge}>Cut</Button>
        <Button
          kind="danger"
          size="small"
          disabled={!edit || selectedId === null}
          on:click={onDeleteClick}>Delete</Button
        >
      </Column>
      <Column>
        {#if targetSet}
          <span class="goto-badge">
            {targetDist.toFixed(1)}m / {targetBearing.toFixed(0)}°
          </span>
          {#if isDriving}
            <button class="goto-btn stop" on:click={stopDrive}>Stop</button>
          {:else}
            <button class="goto-btn drive" on:click={startDrive}>Drive</button>
          {/if}
          <button class="goto-btn clear" on:click={clearTarget}>✕</button>
        {:else if !edit}
          <span class="goto-hint">Click map to set target</span>
        {/if}
      </Column>
    </Row>
  </Grid>
  <div class="map-canvas-wrapper">
    <Canvas on:mapclick={onMapClick}>
      {#if $MapStore && $MapStore.map}
        <Perimeter
          value={$MapStore.map.perimeter}
          perimiterId="map-0-perimeter"
          {edit}
          bind:editItemId
        />
        {#each $MapStore.map.exclusions as exclusion, index}
          <Exclusion
            value={exclusion}
            exclusionId={"map-0-exclusion-" + index}
            {edit}
            bind:editItemId
          />
        {/each}
        <Waypoints
          value={$MapStore.map.waypoints}
          waypointsId="map-0-waypoints"
          {edit}
          bind:editItemId
        />
        <Dockpoints
          value={$MapStore.map.dockpoints}
          dockpointsId="map-0-dockpoints"
          {edit}
          bind:editItemId
        />
        <MowerPosition position={$socketStore.state?.position ?? null} />

        {#if targetSet && targetPos}
          <circle
            cx={targetPos.x}
            cy={-targetPos.y}
            r="0.18"
            class="goto-target-ring"
          />
          <circle
            cx={targetPos.x}
            cy={-targetPos.y}
            r="0.07"
            class="goto-target-dot"
          />
        {/if}

        {#if targetSet && targetPos && mowerPos && (mowerPos.x !== 0 || mowerPos.y !== 0)}
          <line
            x1={mowerPos.x}
            y1={-mowerPos.y}
            x2={targetPos.x}
            y2={-targetPos.y}
            class="goto-line"
          />
        {/if}
      {/if}
    </Canvas>
  </div>
</div>

<style>
  .map-dashboard {
    width: 100%;
    height: 100%;
    padding-top: 48px;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-sizing: border-box;
  }
  .map-canvas-wrapper {
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }

  .goto-badge {
    font-size: 0.75em;
    padding: 3px 8px;
    background: #fff3e0;
    border: 1px solid #ffab00;
    border-radius: 4px;
    color: #b06000;
    font-family: monospace;
    margin-right: 4px;
  }

  .goto-btn {
    padding: 4px 10px;
    border: 1px solid #ccc;
    border-radius: 4px;
    font-size: 0.75em;
    cursor: pointer;
    background: #f4f4f4;

    &.drive {
      background: #e8f5e9;
      border-color: #4caf50;
      color: #2e7d32;
    }

    &.stop {
      background: #ffebee;
      border-color: #ef5350;
      color: #c62828;
    }

    &.clear {
      padding: 4px 8px;
    }
  }

  .goto-hint {
    font-size: 0.75em;
    color: #888;
    font-style: italic;
  }

  :global(.goto-target-ring) {
    fill: none;
    stroke: #e65100;
    stroke-width: 0.04;
    opacity: 0.6;
    animation: goto-pulse 1.5s ease-in-out infinite;
  }

  :global(.goto-target-dot) {
    fill: #e65100;
    stroke: white;
    stroke-width: 0.02;
  }

  :global(.goto-line) {
    stroke: #e65100;
    stroke-width: 0.04;
    stroke-dasharray: 0.12, 0.12;
    opacity: 0.7;
    pointer-events: none;
  }

  @keyframes goto-pulse {
    0%, 100% { transform: scale(1); opacity: 0.6; }
    50% { transform: scale(1.3); opacity: 0.3; }
  }
</style>
