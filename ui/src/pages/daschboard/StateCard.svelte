<script lang="ts">
    import type { DesiredState, State, Stats, ValueDescriptions } from "../../model";
    import { RobotCommandService } from "../../service";
    import MowMotorIcon from "carbon-icons-svelte/lib/TropicalStormTracks.svelte";
    import PlayFilled from "carbon-icons-svelte/lib/PlayFilled.svelte";
    import StopFilled from "carbon-icons-svelte/lib/StopFilled.svelte";
    import Home from "carbon-icons-svelte/lib/Home.svelte";
    import Restart from "carbon-icons-svelte/lib/Restart.svelte";
    import Power from "carbon-icons-svelte/lib/Power.svelte";
    import SkipForward from "carbon-icons-svelte/lib/SkipForwardFilled.svelte";
    import ChartColumn from "carbon-icons-svelte/lib/ChartColumn.svelte";
    import ChartLine from "carbon-icons-svelte/lib/ChartLine.svelte";

    export let state: State | null = null;
    export let stats: Stats | null = null;
    export let desiredState: DesiredState | null = null;
    export let valueDescriptions: ValueDescriptions;

    interface CommandLogEntry {
        time: string;
        action: string;
        success: boolean;
        error?: string;
    }

    let commandLog: CommandLogEntry[] = [];

    function formatTime(): string {
        const now = new Date();
        return now.toLocaleTimeString('de-DE', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    async function send(action: string, payload?: Record<string, any>) {
        let entry: CommandLogEntry = { time: formatTime(), action, success: false };
        try {
            const res = await RobotCommandService.send(action as any, payload);
            entry.success = res.success;
            commandLog = [entry, ...commandLog].slice(0, 8);
        } catch (e: any) {
            entry.error = e.message || String(e);
            commandLog = [entry, ...commandLog].slice(0, 8);
        }
    }
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
                #{state.position?.visible_satellites ?? 0}/{state.position?.visible_satellites_dgps ?? 0}
            </div>
            <div class="label">State</div><div>{valueDescriptions.job[state.job ?? 0]}</div>
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
<article class="state-card">
	<h3>Control</h3>
    <div class="control-buttons">
        <button title="Start" on:click={() => send('start')}><PlayFilled /></button>
        <button title="Stop" on:click={() => send('stop')}><StopFilled /></button>
        <button title="Dock" on:click={() => send('dock')}><Home /></button>
        <button title="Skip Waypoint" on:click={() => send('skipWaypoint')}><SkipForward /></button>
    </div>
    <div class="control-buttons">
        <button title="Reboot Mower" on:click={() => send('reboot')}><Restart /></button>
        <button title="Power Off" on:click={() => send('poweroff')}><Power /></button>
        <button title="Request Stats" on:click={() => send('requestStats')}><ChartColumn /></button>
        <button title="Request Status" on:click={() => send('requestStatus')}><ChartLine /></button>
    </div>
    <div class="toggle-row">
        <label class="toggle-label">
            <input type="checkbox" checked={desiredState?.mower_motor_enabled ?? false}
                   on:change={(e) => send('mowerEnabled', { enabled: e.currentTarget.checked })} />
            Mow Motor
        </label>
        <label class="toggle-label">
            <input type="checkbox" checked={desiredState?.finish_and_restart ?? false}
                   on:change={(e) => send('finishAndRestartEnabled', { enabled: e.currentTarget.checked })} />
            Finish &amp; Restart
        </label>
    </div>
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
{#if commandLog.length > 0}
<article class="state-card command-log">
	<h3>Last Commands</h3>
    <div class="log-list">
        {#each commandLog as entry}
            <div class="log-entry" class:ok={entry.success} class:err={!entry.success || entry.error}>
                <span class="log-time">{entry.time}</span>
                <span class="log-action">{entry.action}</span>
                <span class="log-status">{entry.error ? entry.error : (entry.success ? "OK" : "FAILED")}</span>
            </div>
        {/each}
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

    .result-bar {
        margin-top: 0.5em;
        padding: 0.3em 0.5em;
        background: #e0f7fa;
        border-radius: 3px;
        font-size: 0.85em;
        color: #006064;
    }

    .control-buttons {
        display: flex;
        gap: 8px;
        margin-bottom: 8px;
    }

    .control-buttons button {
        width: 44px;
        height: 44px;
        display: flex;
        align-items: center;
        justify-content: center;
        border: 1px solid #ccc;
        border-radius: 4px;
        background: #f4f4f4;
        cursor: pointer;
        transition: background 0.15s;
    }

    .control-buttons button:hover {
        background: #e0e0e0;
    }

    .control-buttons button:active {
        background: #c6c6c6;
    }

    .toggle-row {
        display: flex;
        gap: 16px;
        margin-top: 4px;
    }

    .toggle-label {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 0.9em;
        cursor: pointer;
    }

    .log-list {
        display: flex;
        flex-direction: column;
        gap: 4px;
    }

    .log-entry {
        display: grid;
        grid-template-columns: 60px 1fr 80px;
        gap: 8px;
        font-size: 0.85em;
        padding: 3px 6px;
        border-radius: 3px;
        background: #f4f4f4;
    }

    .log-entry.ok {
        background: #e6f4ea;
        color: #1e8e3e;
    }

    .log-entry.err {
        background: #fce8e6;
        color: #c5221f;
    }

    .log-time {
        font-family: monospace;
        opacity: 0.7;
    }

    .log-action {
        font-weight: 500;
    }

    .log-status {
        text-align: right;
        font-weight: bold;
    }
</style>
