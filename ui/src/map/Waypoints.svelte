<script lang="ts">
  import { pointsToEdges } from "./geometry";
  import type { Waypoints as WaypointsModel } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";

  export let value: WaypointsModel;
  export let waypointsId: string;
  export let edit: boolean = false;
  export let editItemId: null | string = null;
</script>

{#if value.points.length > 1}
  <polyline
    fill="none"
    stroke="blue"
    stroke-width="0.015"
    points={value.points.map(p => `${p.x},${p.y}`).join(" ")}
  />
{/if}

{#each value.points as point, index}
  {@const s = 0.05}
  <line x1={point.x - s} y1={point.y} x2={point.x + s} y2={point.y} stroke="blue" stroke-width="0.015" />
  <line x1={point.x} y1={point.y - s} x2={point.x} y2={point.y + s} stroke="blue" stroke-width="0.015" />
{/each}

{#if edit}
  {#each pointsToEdges(value.points) as edge, index}
    <Edge
      value={edge}
      mapItemId={waypointsId + "-edge-" + index}
      bind:editItemId
      strokeChoose="blue"
      strokeWidth={0.015}
    />
  {/each}
{/if}
