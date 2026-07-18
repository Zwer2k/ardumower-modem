<script lang="ts">
  import { pointsToEdges } from "./geometry";
  import type { Dockpoints as DockpointsModel, Point as PointType } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";
  import type { MapArea } from "./model";

  export let value: DockpointsModel;
  export let dockpointsId: string;
  export let edit: boolean = false;
  export let editCategory: MapArea = "dockpoints";
  export let editItemId: null | string = null;
  export let drawActive: boolean = false;
  export let onMove: (points: PointType[]) => void = () => {};

  $: isActive = editCategory === "dockpoints";

  function updatePoint(index: number, x: number, y: number) {
    value = {
      ...value,
      points: value.points.map((p, i) => i === index ? { x, y } : p),
    };
    onMove(value.points);
  }
</script>

{#if value.points.length > 1}
  <polyline
    fill="none"
    stroke="orange"
    stroke-width="0.03"
    points={value.points.map(p => `${p.x},${p.y}`).join(" ")}
  />
{/if}

{#if edit && isActive}
  {#each pointsToEdges(value.points) as edge, index}
    <Edge
      value={edge}
      mapItemId={dockpointsId + "-edge-" + index}
      bind:editItemId
      strokeChoose="orange"
      strokeWidth={0.03}
    />
  {/each}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={dockpointsId + "-point-" + index}
      bind:editItemId
      {drawActive}
      strokeChoose="orange"
      r={0.08}
      on:move={(e) => updatePoint(index, e.detail.x, e.detail.y)}
    />
  {/each}
{/if}
