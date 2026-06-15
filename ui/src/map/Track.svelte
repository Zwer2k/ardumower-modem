<script lang="ts">
  import { drivenTrackStore } from "./service";

  $: data = $drivenTrackStore;
  $: trackStr = data?.points?.map(p => `${p.x},${-p.y}`).join(' ') ?? '';
</script>

{#if trackStr}
  <polyline
    points={trackStr}
    fill="none"
    stroke="rgba(0, 200, 255, 0.35)"
    stroke-width="0.04"
    stroke-linecap="round"
    stroke-linejoin="round"
    pointer-events="none"
  />
{/if}
