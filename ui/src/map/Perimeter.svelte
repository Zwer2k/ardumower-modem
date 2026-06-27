<script lang="ts">
  import Edge from "./Edge.svelte";

  import { pointsForPolygon, pointsToEdges } from "./geometry";
  import type { Perimeter as PerimeterModel } from "./model";
  import Point from "./Point.svelte";

  export let value: PerimeterModel;
  export let perimiterId: string;
  export let edit: boolean = false;
  export let editItemId: null | string = null;

  const lineWidth = 0.025;
</script>

<polygon
  fill="green"
  stroke="red"
  stroke-width={lineWidth}
  points={pointsForPolygon(value.points)}
/>

{#if edit}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={perimiterId + "-point-" + index}
      bind:editItemId
      r={0.11}
      on:move={() => value = value}
    />
  {/each}
  {#each pointsToEdges(value.points) as edge, index}
    <Edge
      value={edge}
      mapItemId={perimiterId + "-edge-" + index}
      bind:editItemId
      strokeWidth={0.05}
    />
  {/each}
{/if}
