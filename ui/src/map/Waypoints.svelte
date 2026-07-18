<script lang="ts">
  import { pointsToEdges, edgeArrowPath } from "./geometry";
  import type { Waypoints as WaypointsModel, Point as PointType } from "./model";
  import Point from "./Point.svelte";
  import Edge from "./Edge.svelte";
  import type { MapArea } from "./model";

  export let value: WaypointsModel;
  export let waypointsId: string;
  export let edit: boolean = false;
  export let editCategory: MapArea = "waypoints";
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
  const pointRadiusMm = 20; // 1.5 mm visible waypoint diameter in edit mode
  const hitExtraMm = 0;     // extra hit area beyond the visible point
  const pointRadius = pointRadiusMm / 1000;
  const hitRadius = (pointRadiusMm + hitExtraMm) / 1000;
  $: isActive = editCategory === "waypoints";
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

{#if edit && isActive}
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

{#each value.points as point, index}
  <circle cx={point.x} cy={point.y} r={pointRadius} fill="blue" pointer-events="none" />
{/each}

{#if edit && isActive}
  {#each value.points as point, index}
    <Point
      value={point}
      mapItemId={waypointsId + "-point-" + index}
      bind:editItemId
      {drawActive}
      strokeChoose="blue"
      fill="blue"
      fillActive="red"
      fillChoose="blue"
      fillPassive="blue"
      r={pointRadius}
      hitR={hitRadius}
      on:move={(e) => updatePoint(index, e.detail.x, e.detail.y)}
    />
  {/each}
{/if}
