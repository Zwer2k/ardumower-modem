<script lang="ts">
  import Edge from "./Edge.svelte";

  import { pointsForPolygon, pointsToEdges } from "./geometry";
  import type { Perimeter as PerimeterModel, Point as PointType } from "./model";
  import Point from "./Point.svelte";

  export let value: PerimeterModel;
  export let perimiterId: string;
  export let edit: boolean = false;
  export let editItemId: null | string = null;

  export let onMove: (points: PointType[]) => void = () => {};

  const lineWidth = 0.025;

  function updatePoint(index: number, x: number, y: number) {
    value = {
      ...value,
      points: value.points.map((p, i) => i === index ? { x, y } : p),
    };
    onMove(value.points);
  }
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
      on:move={(e) => updatePoint(index, e.detail.x, e.detail.y)}
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
