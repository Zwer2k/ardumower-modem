<script lang="ts">
  import { pointsToEdges, edgeArrowPath } from "./geometry";
  import type { Waypoints as WaypointsModel, Point as PointType } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";

  export let value: WaypointsModel;
  export let waypointsId: string;
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
  const pointRadius = 0.0115; // ~1.5 mm visible waypoint size in edit mode
  const hitRadius = 0.031;     // larger invisible hit area for dragging
  $: arrowD = edgeArrowPath(value.points, arrowSize, arrowHw);
</script>

{#if value.points.length > 1}
  <polyline
    fill="none"
    stroke="blue"
    stroke-width={lineWidth}
    points={value.points.map(p => `${p.x},${p.y}`).join(" ")}
  />

  <path d={arrowD} stroke="blue" stroke-width={lineWidth} fill="none" stroke-linecap="round" stroke-linejoin="round" />
{/if}

{#if !edit}
  {#each value.points as point, index}
    <circle cx={point.x} cy={point.y} r={pointRadius} fill="blue" />
  {/each}
{/if}

{#if edit}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={waypointsId + "-point-" + index}
      bind:editItemId
      {drawActive}
      strokeChoose="blue"
      fill="blue"
      fillActive="blue"
      fillPassive="blue"
      r={pointRadius}
      hitR={hitRadius}
      on:move={(e) => updatePoint(index, e.detail.x, e.detail.y)}
    />
  {/each}
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
