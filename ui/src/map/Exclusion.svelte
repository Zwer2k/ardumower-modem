<script lang="ts">
  import { pointsForPolygon, pointsToEdges, polygonArrowPath } from "./geometry";
  import type { Exclusion as ExclusionModel, Point as PointType } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";

  export let value: ExclusionModel;
  export let exclusionId: string;
  export let edit: boolean = false;
  export let editItemId: null | string = null;
  export let drawActive: boolean = false;

  export let onMove: (points: PointType[]) => void = () => {};

  function updatePoint(index: number, x: number, y: number) {
    value = {
      ...value,
      points: value.points.map((p, i) => i === index ? { x, y } : p),
    };
    onMove(value.points);
  }

  const arrowSize = 0.06;
  const arrowHw = 0.03;
  const lineWidth = 0.015;
  $: arrowD = polygonArrowPath(value.points, arrowSize, arrowHw);
</script>

<polygon
  fill="red"
  stroke="black"
  stroke-width={lineWidth}
  points={pointsForPolygon(value.points)}
/>

<path d={arrowD} stroke="black" stroke-width={lineWidth} fill="none" stroke-linecap="round" stroke-linejoin="round" />

{#if edit}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={exclusionId + "-point-" + index}
      bind:editItemId
      {drawActive}
      on:move={(e) => updatePoint(index, e.detail.x, e.detail.y)}
    />
  {/each}
  {#each pointsToEdges(value.points) as edge, index}
    <Edge
      value={edge}
      mapItemId={exclusionId + "-edge-" + index}
      bind:editItemId
    />
  {/each}
{/if}
