<script lang="ts">
    import type { DesiredState, State, Stats, ValueDescriptions } from "../../../model";
    import { SensorDescriptions } from "../../../model";
    export let state: State | null = null;
    export let stats: Stats | null = null;
    export let desiredState: DesiredState | null = null;
    export let valueDescriptions: ValueDescriptions;
</script>

<article class="state-card">
	<h3>State</h3>
    {#if state != null}
        <div class="item-list">
            <div class="label">Battery</div><div>{(state.battery_voltage ?? 0).toFixed(1)}V/{(state.amps ?? 0).toFixed(1)}A</div>
            <div class="label">Position</div><div>{(state.position?.x ?? 0).toFixed(4)}E {(state.position?.y ?? 0).toFixed(2)}N</div>
            <div class="label">Satelite</div>
            <div>
                {valueDescriptions.posSolution[state.position?.solution ?? 0]}/{(state.position?.age ?? 0).toFixed(1)}s/{state.position?.accuracy ?? 0}m
                #{state.position?.visible_satellites_dgps ?? 0}/{state.position?.visible_satellites ?? 0}
            </div>
            <div class="label">State</div><div>{valueDescriptions.job[state.job ?? 0]}</div>
            <div class="label">Sensor</div><div>{SensorDescriptions[state.sensor ?? 0] ?? "unknown"}</div>
            <div class="label">Map CRC</div><div>{state.map_crc ?? 0}</div>
        </div>
    {/if}
    {#if desiredState != null}
        <div class="item-list">
            <div class="label">OP</div><div>{valueDescriptions.job[desiredState.op ?? 0]}</div>
            <div class="label">Speed</div><div>{desiredState.speed ?? 0} m/s</div>
            <div class="label">Fix Timeout</div><div>{desiredState.fix_timeout ?? 0}s</div>
        </div>
    {/if}
</article>
{#if stats != null}
<article class="state-card">
	<h3>Mower Stats</h3>
    <div class="item-list">
        <div class="label">Mow dist</div><div>{(stats.mow_traveled ?? 0).toFixed(1)} m</div>
        <div class="label">Durations</div><div>idle:{stats.idle ?? 0} chg:{stats.charge ?? 0} mow:{stats.mow ?? 0}</div>
        <div class="label">Mow detail</div><div>inv:{stats.mow_invalid ?? 0} flt:{stats.mow_float ?? 0} fix:{stats.mow_fix ?? 0}</div>
        <div class="label">Recoveries</div><div>inv:{stats.invalid_recoveries ?? 0} flt→fix:{stats.float_recoveries ?? 0} imu:{stats.imu_triggered ?? 0}</div>
        <div class="label">Obstacles</div><div>tot:{stats.obstacles ?? 0} sonar:{stats.sonar_triggered ?? 0} bmp:{stats.bumper_triggered ?? 0} gps:{stats.gps_motion_timeout ?? 0}</div>
        <div class="label">GPS errors</div><div>gps:{stats.gps_chk_sum_errors ?? 0} dgps:{stats.dgps_chk_sum_errors ?? 0} jumps:{stats.gps_jumps ?? 0}</div>
        <div class="label">Max DGPS</div><div>{(stats.max_dpgs_age ?? 0).toFixed(1)} s</div>
        <div class="label">Cycle time</div><div>{(stats.max_cycle ?? 0).toFixed(2)} s</div>
        <div class="label">Serial buf</div><div>{stats.serial_buffer_size ?? 0} B</div>
        <div class="label">Free mem</div><div>{stats.free_memory ?? 0} B</div>
        <div class="label">Reset</div><div>{stats.reset_cause ?? 0}</div>
        <div class="label">Temp</div><div>{(stats.temp_min ?? 0).toFixed(1)}°C / {(stats.temp_max ?? 0).toFixed(1)}°C</div>
    </div>
</article>
{/if}
<style lang="scss">
	.state-card {
		width: 100%;
		border: 1px solid #aaa;
		border-radius: 2px;
		box-shadow: 2px 2px 8px rgba(0,0,0,0.1);
		padding: 1em;
        margin-bottom: 5px;
	}

	h3 {
		padding: 0 0 0.2em 0;
		margin: 0 0 1em 0;
		border-bottom: 1px solid #ff3e00
	}

	.item-list {
		display: grid;
        grid-template-columns: minmax(20%, max-content) auto;
	}

    .label {
        font-weight: bold;
        margin-right: 10px;
    }

</style>
