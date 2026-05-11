<script lang="ts">
  import type { SensorSummary } from "../../model";

  export let summary: SensorSummary | null = null;

  interface SensorEvent {
    time: string;
    text: string;
    kind: "trigger" | "clear" | "info";
  }

  let events: SensorEvent[] = [];
  let lastSummary: SensorSummary | null = null;

  function formatTime(ts: number): string {
    const d = new Date(ts);
    return d.toLocaleTimeString("de-DE", { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
  }

  function nowTime(): string {
    return new Date().toLocaleTimeString("de-DE", { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
  }

  function pushEvent(text: string, kind: "trigger" | "clear" | "info") {
    events = [{ time: nowTime(), text, kind }, ...events].slice(0, 20);
  }

  function boolChanged(prev: boolean, curr: boolean, name: string) {
    if (!prev && curr) pushEvent(`${name} ausgelöst`, "trigger");
    if (prev && !curr) pushEvent(`${name} frei`, "clear");
  }

  $: {
    if (summary != null && lastSummary != null) {
      boolChanged(lastSummary.sonar_obstacle, summary.sonar_obstacle, "Sonar");
      boolChanged(lastSummary.bumper_obstacle, summary.bumper_obstacle, "Bumper");
      boolChanged(lastSummary.lidar_obstacle, summary.lidar_obstacle, "Lidar");
      boolChanged(lastSummary.lift_triggered, summary.lift_triggered, "Lift");
      boolChanged(lastSummary.rain_triggered, summary.rain_triggered, "Rain");

      if (lastSummary.sonar_near_obstacle !== summary.sonar_near_obstacle) {
        pushEvent(`Sonar near ${summary.sonar_near_obstacle ? "erkannt" : "frei"}`, summary.sonar_near_obstacle ? "trigger" : "clear");
      }
      if (lastSummary.bumper_left !== summary.bumper_left) {
        pushEvent(`Bumper links ${summary.bumper_left ? "ausgelöst" : "frei"}`, summary.bumper_left ? "trigger" : "clear");
      }
      if (lastSummary.bumper_right !== summary.bumper_right) {
        pushEvent(`Bumper rechts ${summary.bumper_right ? "ausgelöst" : "frei"}`, summary.bumper_right ? "trigger" : "clear");
      }
    }
    if (summary != null) {
      lastSummary = { ...summary };
    }
  }
</script>

<article class="state-card">
  <h3>Sensor Status</h3>
  {#if summary != null}
    <div class="sensor-grid">
      <div class="sensor-group">
        <div class="group-title">Sonar (cm)</div>
        <div class="sensor-values">
          <span class:value-active={summary.sonar_left < 30}>L:{summary.sonar_left.toFixed(0)}</span>
          <span class:value-active={summary.sonar_center < 30}>C:{summary.sonar_center.toFixed(0)}</span>
          <span class:value-active={summary.sonar_right < 30}>R:{summary.sonar_right.toFixed(0)}</span>
        </div>
        <div class="sensor-flags">
          {#if summary.sonar_obstacle}<span class="flag trigger">Obstacle</span>{/if}
          {#if summary.sonar_near_obstacle}<span class="flag near">Near</span>{/if}
        </div>
      </div>
      <div class="sensor-group">
        <div class="group-title">Bumper</div>
        <div class="sensor-flags">
          {#if summary.bumper_left}<span class="flag trigger">Links</span>{/if}
          {#if summary.bumper_right}<span class="flag trigger">Rechts</span>{/if}
          {#if summary.bumper_obstacle}<span class="flag trigger">Obstacle</span>{/if}
          {#if summary.bumper_near_obstacle}<span class="flag near">Near</span>{/if}
        </div>
      </div>
      <div class="sensor-group">
        <div class="group-title">Lidar</div>
        <div class="sensor-flags">
          {#if summary.lidar_obstacle}<span class="flag trigger">Obstacle</span>{/if}
          {#if summary.lidar_near_obstacle}<span class="flag near">Near</span>{/if}
        </div>
      </div>
      <div class="sensor-group compact">
        <div class="sensor-flags">
          {#if summary.lift_triggered}<span class="flag trigger">Lift</span>{/if}
          {#if summary.rain_triggered}<span class="flag info">Rain</span>{/if}
        </div>
      </div>
    </div>
  {:else}
    <div class="no-data">Keine Sensordaten verfügbar</div>
  {/if}
</article>

{#if events.length > 0}
<article class="state-card command-log">
  <h3>Sensor Events</h3>
  <div class="log-list">
    {#each events as ev}
      <div class="log-entry" class:trigger={ev.kind === "trigger"} class:clear={ev.kind === "clear"}>
        <span class="log-time">{ev.time}</span>
        <span class="log-action">{ev.text}</span>
      </div>
    {/each}
  </div>
</article>
{/if}

<style lang="scss">
  .sensor-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .sensor-group {
    border: 1px solid #ddd;
    border-radius: 4px;
    padding: 6px;
    background: #fafafa;
  }
  .group-title {
    font-size: 0.8em;
    font-weight: bold;
    color: #555;
    margin-bottom: 4px;
  }
  .sensor-values {
    display: flex;
    gap: 8px;
    font-family: monospace;
    font-size: 0.9em;
  }
  .sensor-values span {
    padding: 2px 4px;
    border-radius: 3px;
    background: #e8e8e8;
  }
  .sensor-values .value-active {
    background: #ffcccc;
    color: #c5221f;
    font-weight: bold;
  }
  .sensor-flags {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
    margin-top: 4px;
  }
  .flag {
    font-size: 0.75em;
    padding: 2px 6px;
    border-radius: 3px;
    font-weight: 500;
  }
  .flag.trigger {
    background: #fce8e6;
    color: #c5221f;
  }
  .flag.near {
    background: #fff4e5;
    color: #b06000;
  }
  .flag.info {
    background: #e0f7fa;
    color: #006064;
  }
  .no-data {
    color: #888;
    font-size: 0.9em;
    text-align: center;
    padding: 8px 0;
  }
  .log-entry.trigger {
    background: #fce8e6;
    color: #c5221f;
  }
  .log-entry.clear {
    background: #e6f4ea;
    color: #1e8e3e;
  }
</style>
