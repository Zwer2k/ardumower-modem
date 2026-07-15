<script lang="ts">
  export let targetPos: { x: number; y: number } | null;
  export let mowerPos: { x: number; y: number } | null;
  export let showTarget: boolean;
  export let showLine: boolean;
</script>

{#if showTarget && targetPos}
  <circle
    cx={targetPos.x}
    cy={targetPos.y}
    r="0.18"
    fill="none"
    stroke="#e65100"
    stroke-width="0.04"
    opacity="0.6"
  />
  <circle
    cx={targetPos.x}
    cy={targetPos.y}
    r="0.07"
    fill="#e65100"
    stroke="white"
    stroke-width="0.02"
  />
{/if}

{#if showLine && targetPos && mowerPos}
  <line
    x1={mowerPos.x}
    y1={-mowerPos.y}
    x2={targetPos.x}
    y2={targetPos.y}
    stroke="#e65100"
    stroke-width="0.04"
    stroke-dasharray="0.12, 0.12"
    opacity="0.7"
    pointer-events="none"
  />
{/if}
