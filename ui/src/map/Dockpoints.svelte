<script lang="ts">
  import { pointsToEdges } from "./geometry";
  import type { Dockpoints as DockpointsModel } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";

  export let value: DockpointsModel;
  export let dockpointsId: string;
  export let edit: boolean = false;
  export let editItemId: null | string = null;
</script>

{#if value.points.length > 1}
  <polyline
    fill="none"
    stroke="orange"
    stroke-width="0.03"
    marker-mid="url(#arrow-dockpoints)"
    marker-end="url(#arrow-dockpoints)"
    points={value.points.map(p => `${p.x},${p.y}`).join(" ")}
  />
{/if}

{#if edit}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={dockpointsId + "-point-" + index}
      bind:editItemId
      strokeChoose="orange"
      r={0.11}
      on:move={() => value = value}
    />
  {/each}
  {#each pointsToEdges(value.points) as edge, index}
    <Edge
      value={edge}
      mapItemId={dockpointsId + "-edge-" + index}
      bind:editItemId
      strokeChoose="orange"
      strokeWidth={0.03}
    />
  {/each}
{/if}
