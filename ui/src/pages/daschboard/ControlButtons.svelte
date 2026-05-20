<script lang="ts">
    import type { DesiredState } from "../../model";
    import { RobotCommandService } from "../../service";
    import PlayFilled from "carbon-icons-svelte/lib/PlayFilled.svelte";
    import StopFilled from "carbon-icons-svelte/lib/StopFilled.svelte";
    import Home from "carbon-icons-svelte/lib/Home.svelte";
    import Restart from "carbon-icons-svelte/lib/Restart.svelte";
    import Power from "carbon-icons-svelte/lib/Power.svelte";
    import SkipForward from "carbon-icons-svelte/lib/SkipForwardFilled.svelte";
    import ChartColumn from "carbon-icons-svelte/lib/ChartColumn.svelte";
    import ChartLine from "carbon-icons-svelte/lib/ChartLine.svelte";

    let { desiredState = null }: { desiredState?: DesiredState | null } = $props();

    let commandLog: { time: string; action: string; success: boolean; error?: string }[] = [];

    function formatTime(): string {
        const now = new Date();
        return now.toLocaleTimeString('de-DE', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    async function send(action: string, payload?: Record<string, any>) {
        let entry = { time: formatTime(), action, success: false };
        try {
            const res = await RobotCommandService.send(action as any, payload);
            entry.success = res.success;
            commandLog = [entry, ...commandLog].slice(0, 8);
        } catch (e: any) {
            entry.error = e.message || String(e);
            commandLog = [entry, ...commandLog].slice(0, 8);
        }
    }

    async function sendCustom(cmd: string, displayAction: string) {
        let entry = { time: formatTime(), action: displayAction, success: false };
        try {
            const res = await RobotCommandService.send('customCmd', { cmd });
            entry.success = res.success;
            commandLog = [entry, ...commandLog].slice(0, 8);
        } catch (e: any) {
            entry.error = e.message || String(e);
            commandLog = [entry, ...commandLog].slice(0, 8);
        }
    }

    // ─── Slider values ──────────────────────────────────────────────────────
    let speedVal = $state(desiredState?.speed ?? 0.2);
    let fixTimeoutVal = $state(desiredState?.fix_timeout ?? 120);
    let mowHeightVal = $state(55);     // mm, typical range 30-85
    let wayPercVal = $state(100);      // %, typical range 0-200

    function onSpeedChange(e: Event) {
        const val = parseFloat((e.target as HTMLInputElement).value);
        speedVal = val;
        send('changeSpeed', { speed: val });
    }

    function onFixTimeoutChange(e: Event) {
        const val = parseInt((e.target as HTMLInputElement).value, 10);
        fixTimeoutVal = val;
        send('setFixTimeout', { timeout: val });
    }

    function onMowHeightChange(e: Event) {
        const val = parseInt((e.target as HTMLInputElement).value, 10);
        mowHeightVal = val;
        sendCustom(`AT+S2,${val}`, `mowHeight=${val}`);
    }

    function onWayPercChange(e: Event) {
        const val = parseInt((e.target as HTMLInputElement).value, 10);
        wayPercVal = val;
        sendCustom(`AT+W,${val}`, `wayPerc=${val}`);
    }
</script>

<div class="rc-controls">
    <div class="control-row">
        <button title="Start" onclick={() => send('start')}><PlayFilled /></button>
        <button title="Stop" onclick={() => send('stop')}><StopFilled /></button>
        <button title="Dock" onclick={() => send('dock')}><Home /></button>
        <button title="Skip Waypoint" onclick={() => send('skipWaypoint')}><SkipForward /></button>
    </div>
    <div class="control-row">
        <button title="Reboot Mower" onclick={() => send('reboot')}><Restart /></button>
        <button title="Power Off" onclick={() => send('poweroff')}><Power /></button>
        <button title="Request Stats" onclick={() => send('requestStats')}><ChartColumn /></button>
        <button title="Request Status" onclick={() => send('requestStatus')}><ChartLine /></button>
    </div>
    <div class="toggle-row">
        <label class="toggle-label">
            <input type="checkbox" checked={desiredState?.mower_motor_enabled ?? false}
                   onchange={(e) => send('mowerEnabled', { enabled: e.currentTarget.checked })} />
            Mow Motor
        </label>
        <label class="toggle-label">
            <input type="checkbox" checked={desiredState?.finish_and_restart ?? false}
                   onchange={(e) => send('finishAndRestartEnabled', { enabled: e.currentTarget.checked })} />
            Finish &amp; Restart
        </label>
        <label class="toggle-label">
            <input type="checkbox"
                   onchange={(e) => send('sonarEnabled', { enabled: e.currentTarget.checked })} />
            Sonar
        </label>
    </div>

    <!-- Sliders -->
    <div class="slider-section">
        <label class="slider-group">
            <span class="slider-label">Speed</span>
            <input type="range" class="slider-input"
                   min="0.01" max="0.60" step="0.01"
                   value={speedVal}
                   onchange={onSpeedChange} />
            <span class="slider-value">{speedVal.toFixed(2)} m/s</span>
        </label>
        <label class="slider-group">
            <span class="slider-label">Fix T/O</span>
            <input type="range" class="slider-input"
                   min="0" max="300" step="1"
                   value={fixTimeoutVal}
                   onchange={onFixTimeoutChange} />
            <span class="slider-value">{fixTimeoutVal}s</span>
        </label>
        <label class="slider-group">
            <span class="slider-label">Mow Ht</span>
            <input type="range" class="slider-input"
                   min="30" max="85" step="1"
                   value={mowHeightVal}
                   onchange={onMowHeightChange} />
            <span class="slider-value">{mowHeightVal} mm</span>
        </label>
        <label class="slider-group">
            <span class="slider-label">Way %</span>
            <input type="range" class="slider-input"
                   min="0" max="200" step="1"
                   value={wayPercVal}
                   onchange={onWayPercChange} />
            <span class="slider-value">{wayPercVal}%</span>
        </label>
    </div>

    {#if commandLog.length > 0}
        <div class="mini-log">
            {#each commandLog as entry}
                <div class="log-entry" class:ok={entry.success} class:err={!entry.success || entry.error}>
                    <span class="log-time">{entry.time}</span>
                    <span class="log-action">{entry.action}</span>
                    <span class="log-status">{entry.error ? entry.error : (entry.success ? "OK" : "FAIL")}</span>
                </div>
            {/each}
        </div>
    {/if}
</div>

<style lang="scss">
    .rc-controls {
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .control-row {
        display: flex;
        gap: 8px;
        justify-content: center;
    }

    .control-row button {
        width: 40px;
        height: 40px;
        display: flex;
        align-items: center;
        justify-content: center;
        border: 1px solid #ccc;
        border-radius: 4px;
        background: #f4f4f4;
        cursor: pointer;
        transition: background 0.15s;
    }

    .control-row button:hover {
        background: #e0e0e0;
    }

    .control-row button:active {
        background: #c6c6c6;
    }

    .toggle-row {
        display: flex;
        gap: 12px;
        justify-content: center;
        margin-top: 4px;
    }

    .toggle-label {
        display: flex;
        align-items: center;
        gap: 4px;
        font-size: 0.8em;
        cursor: pointer;
    }

    /* ─── Sliders ────────────────────────────────────────────────────────── */
    .slider-section {
        display: flex;
        flex-direction: column;
        gap: 6px;
        margin-top: 4px;
        padding-top: 8px;
        border-top: 1px solid #eee;
    }

    .slider-group {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 0.75em;
    }

    .slider-label {
        width: 50px;
        text-align: right;
        color: #555;
        flex-shrink: 0;
        font-weight: 500;
    }

    .slider-input {
        flex: 1;
        height: 5px;
        -webkit-appearance: none;
        appearance: none;
        background: #ddd;
        border-radius: 3px;
        outline: none;
        cursor: pointer;
    }

    .slider-input::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 14px;
        height: 14px;
        border-radius: 50%;
        background: #006064;
        cursor: pointer;
    }

    .slider-input::-moz-range-thumb {
        width: 14px;
        height: 14px;
        border-radius: 50%;
        background: #006064;
        cursor: pointer;
        border: none;
    }

    .slider-value {
        width: 65px;
        text-align: left;
        font-family: monospace;
        color: #333;
        flex-shrink: 0;
    }

    .mini-log {
        display: flex;
        flex-direction: column;
        gap: 3px;
        font-size: 0.75em;
        margin-top: 4px;
        max-height: 100px;
        overflow-y: auto;
    }

    .log-entry {
        display: grid;
        grid-template-columns: 50px 1fr 50px;
        gap: 6px;
        padding: 2px 6px;
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
        font-size: 0.9em;
    }

    .log-action {
        font-weight: 500;
    }

    .log-status {
        text-align: right;
        font-weight: bold;
    }
</style>
