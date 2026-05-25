<script lang="ts">
  import type { Position } from "../model";

  export let position: Position | null = null;

  // y must be inverted because the map uses y = -Y (SVG coordinates)
  $: x = position?.x ?? 0;
  $: y = position != null ? -position.y : 0;
  $: delta = position?.delta ?? 0;
  $: hasPos = position != null && (x !== 0 || y !== 0);
  $: accuracy = position?.accuracy ?? 0;

  // Arrow length in map units
  const arrowLen = 0.35;
  const arrowWing = 0.12;

  function arrowPoints(cx: number, cy: number, deg: number): string {
    // delta: 0 = north (SVG negative y), increasing clockwise
    const rad = ((deg - 90) * Math.PI) / 180;
    const tipX = cx + Math.cos(rad) * arrowLen;
    const tipY = cy + Math.sin(rad) * arrowLen;

    // Wing points (perpendicular, pointing backward from tip)
    const wrad1 = rad + Math.PI * 0.8;
    const wing1X = cx + Math.cos(wrad1) * arrowWing;
    const wing1Y = cy + Math.sin(wrad1) * arrowWing;

    const wrad2 = rad - Math.PI * 0.8;
    const wing2X = cx + Math.cos(wrad2) * arrowWing;
    const wing2Y = cy + Math.sin(wrad2) * arrowWing;

    return `${wing1X},${wing1Y} ${tipX},${tipY} ${wing2X},${wing2Y}`;
  }
</script>

{#if hasPos}
  <g class="mower-position">
    <!-- Position accuracy circle (if available) -->
    {#if accuracy > 0}
      <circle
        cx={x}
        cy={y}
        r={accuracy}
        fill="rgba(0, 200, 255, 0.08)"
        stroke="rgba(0, 200, 255, 0.35)"
        stroke-width="0.02"
      />
    {/if}

    <!-- Outer ring for visibility -->
    <circle
      cx={x}
      cy={y}
      r="0.18"
      fill="none"
      stroke="rgba(0, 200, 255, 0.5)"
      stroke-width="0.03"
    />

    <!-- Mower body -->
    <circle
      cx={x}
      cy={y}
      r="0.12"
      fill="#00c8ff"
      stroke="#005f73"
      stroke-width="0.03"
    />

    <!-- Direction arrow -->
    <polyline
      points={arrowPoints(x, y, delta)}
      fill="none"
      stroke="#005f73"
      stroke-width="0.06"
      stroke-linecap="round"
      stroke-linejoin="round"
    />

    <!-- Center dot -->
    <circle
      cx={x}
      cy={y}
      r="0.04"
      fill="white"
    />
  </g>
{/if}

<style>
  .mower-position {
    pointer-events: none;
  }
</style>
