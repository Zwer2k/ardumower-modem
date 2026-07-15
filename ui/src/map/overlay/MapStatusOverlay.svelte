<script lang="ts">
  import type { SocketState } from "../../stores/socket";

  export let compassRotation: number;
  export let socketState: SocketState;
  export let perimeterPoints: number;
  export let exclusionPoints: number[];
  export let dockpointsPoints: number;
  export let waypointsPoints: number;
  export let totalPoints: number;
  export let needsUpload: boolean;
  export let onCompassDown: (e: MouseEvent) => void;
</script>

<div class="map-top-right">
  <button class="compass-btn" on:mousedown={onCompassDown} title="Karte drehen (ziehen für feine Ausrichtung)">
    <svg viewBox="-12 -12 24 24" width="28" height="28">
      <g transform="rotate({compassRotation})">
        <circle cx="0" cy="0" r="10" fill="white" stroke="#999" stroke-width="1.5"/>
        <polygon points="0,-8 -4,0 0,-2 4,0" fill="#d32f2f"/>
        <polygon points="0,8 -4,0 0,2 4,0" fill="#999"/>
      </g>
    </svg>
  </button>
  <div class="map-point-counts">
    <div><strong>Area:</strong> {(socketState.currentMapMeta?.area ?? 0).toFixed(1)} m²</div>
    <div><strong>Perimeter:</strong> {perimeterPoints}</div>
    {#each exclusionPoints as ep, i}
      <div><strong>Excl #{i}:</strong> {ep}</div>
    {/each}
    <div><strong>Dock:</strong> {dockpointsPoints}</div>
    <div><strong>Way:</strong> {waypointsPoints}</div>
    <div><strong>Total:</strong> {totalPoints}</div>
    <div class:sync-ok={!needsUpload} class:sync-warn={needsUpload}>{needsUpload ? '⚠' : '✓'} {needsUpload ? 'not synced' : 'synced'}</div>
  </div>
</div>

<style>
  .map-top-right {
    position: absolute;
    top: 0.5rem;
    right: 0.5rem;
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 0.5rem;
    z-index: 10;
    pointer-events: none;
  }
  .map-top-right > * {
    pointer-events: auto;
  }
  .compass-btn {
    background: none;
    border: none;
    cursor: pointer;
    padding: 0;
  }
  .map-point-counts {
    background: rgba(255, 255, 255, 0.9);
    border: 1px solid #e0e0e0;
    border-radius: 4px;
    padding: 0.5rem;
    font-size: 0.75rem;
    line-height: 1.4;
    min-width: 120px;
  }
  .sync-ok {
    color: #2e7d32;
  }
  .sync-warn {
    color: #c62828;
  }
</style>
