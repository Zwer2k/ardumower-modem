<script lang="ts">
    import { slide } from "svelte/transition";
    import type { DesiredState } from "../../../model";
    import { RobotCommandService } from "../../../service";
    import { socketStore } from "../../../stores/socket";
    import { MapStore } from "../../../map/service";
    import PlayFilled from "carbon-icons-svelte/lib/PlayFilled.svelte";
    import StopFilled from "carbon-icons-svelte/lib/StopFilled.svelte";
    import Home from "carbon-icons-svelte/lib/Home.svelte";
    import Restart from "carbon-icons-svelte/lib/Restart.svelte";
    import Power from "carbon-icons-svelte/lib/Power.svelte";
    import SkipForward from "carbon-icons-svelte/lib/SkipForwardFilled.svelte";
    import ChartColumn from "carbon-icons-svelte/lib/ChartColumn.svelte";
    import ChartLine from "carbon-icons-svelte/lib/ChartLine.svelte";

    let { desiredState = null }: { desiredState?: DesiredState | null } = $props();

    let commandLog: { time: string; action: string; success: boolean; error?: string }[] = $state([]);

    function formatTime(): string {
        const now = new Date();
        return now.toLocaleTimeString('de-DE', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    async function send(action: string, payload?: Record<string, any>) {
        let entry = { time: formatTime(), action, success: false, error: undefined as string | undefined };
        try {
            const res = await RobotCommandService.send(action as any, payload);
            entry.success = res.success;
            commandLog = [entry, ...commandLog].slice(0, 8);
        } catch (e: any) {
            entry.error = e.message || String(e);
            commandLog = [entry, ...commandLog].slice(0, 8);
        }
    }

    // ─── Slider values (initialize from props once, then independent) ──────
    let speedVal = $state(0.2);
    let fixTimeoutVal = $state(120);
    // svelte-ignore state_referenced_locally
    if (desiredState) { speedVal = desiredState.speed ?? speedVal; fixTimeoutVal = desiredState.fix_timeout ?? fixTimeoutVal; }
    let mowHeightVal = $state(55);     // mm, typical range 30-85
    let wayPercVal = $state(100);      // %, typical range 0-100
    let wayPercManuallySetAt = $state<number | null>(null);
    const WAY_PERC_MIRROR_PAUSE_MS = 3000;

    // mirror mower's current waypoint index into the Way % slider, unless recently changed manually
    $effect(() => {
        const total = $MapStore?.map?.waypoints?.points?.length ?? 0;
        const idx = $socketStore?.state?.position?.mow_point_index ?? -1;
        if (total > 0 && idx >= 0) {
            if (wayPercManuallySetAt && (Date.now() - wayPercManuallySetAt < WAY_PERC_MIRROR_PAUSE_MS)) {
                return;
            }
            const target = parseFloat((((idx + 1) / total) * 100).toFixed(1));
            if (Math.abs(target - wayPercVal) > 0.05) wayPercVal = target;
        }
    });

    // tune parameters via AT+CT (index -> value)
    let tuneExpanded = $state(false);
    let motorPidKpVal = $state(2.0);     // param 4
    let motorPidKiVal = $state(0.0);     // param 5
    let motorPidKdVal = $state(0.02);    // param 6
    let motorPidTfVal = $state(0.0);     // param 7 (low-pass filter)
    let motorPidRampVal = $state(100);   // param 8 (output ramp)
    let motorPidLimitVal = $state(255);  // param 9 (pwmMax)
    let stanleyNormalPVal = $state(1.0); // param 0
    let stanleyNormalKVal = $state(1.0); // param 1
    let stanleySlowPVal = $state(0.5);   // param 2
    let stanleySlowKVal = $state(0.5);   // param 3

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
        send('changeMowHeight', { height: val });
    }

    function onWayPercChange(e: Event) {
        const val = parseFloat((e.target as HTMLInputElement).value);
        setWayPerc(val);
    }

    function adjustWayPerc(delta: number) {
        const next = Math.max(0, Math.min(100, wayPercVal + delta));
        setWayPerc(next);
    }

    function setWayPerc(val: number) {
        wayPercVal = val;
        wayPercManuallySetAt = Date.now();
        send('changeWayPerc', { perc: val / 100 });
    }

    function onTuneParam(index: number) {
        return (e: Event) => {
            const val = parseFloat((e.target as HTMLInputElement).value);
            if (index === 0) stanleyNormalPVal = val;
            else if (index === 1) stanleyNormalKVal = val;
            else if (index === 2) stanleySlowPVal = val;
            else if (index === 3) stanleySlowKVal = val;
            else if (index === 4) motorPidKpVal = val;
            else if (index === 5) motorPidKiVal = val;
            else if (index === 6) motorPidKdVal = val;
            else if (index === 7) motorPidTfVal = val;
            else if (index === 8) motorPidRampVal = val;
            else if (index === 9) motorPidLimitVal = val;
            send('tuneParam', { index, value: val });
        };
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
        <div class="slider-group way-perc-group">
            <span class="slider-label">Way %</span>
            <div class="way-perc-control">
                <button type="button" class="step-btn" onclick={() => adjustWayPerc(-0.1)} aria-label="Way % verringern">−</button>
                <input type="range" class="slider-input"
                       min="0" max="100" step="0.1"
                       value={wayPercVal}
                       onchange={onWayPercChange} />
                <button type="button" class="step-btn" onclick={() => adjustWayPerc(0.1)} aria-label="Way % erhöhen">+</button>
            </div>
            <span class="slider-value">{wayPercVal.toFixed(1)}%</span>
        </div>
    </div>

    <div class="slider-section tune-section">
        <button type="button" class="tune-toggle" onclick={() => tuneExpanded = !tuneExpanded}>
            Advanced Tuning {tuneExpanded ? '▲' : '▼'}
        </button>
        {#if tuneExpanded}
            <div class="tune-groups" transition:slide={{ duration: 150 }}>
                <div class="tune-group">
                    <span class="slider-label">Stanley Control</span>
                    <label class="slider-group">
                        <span class="slider-label sub">Normal P</span>
                        <input type="range" class="slider-input"
                               min="0" max="5" step="0.01"
                               value={stanleyNormalPVal}
                               onchange={onTuneParam(0)} />
                        <span class="slider-value">{stanleyNormalPVal.toFixed(2)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Normal K</span>
                        <input type="range" class="slider-input"
                               min="0" max="5" step="0.01"
                               value={stanleyNormalKVal}
                               onchange={onTuneParam(1)} />
                        <span class="slider-value">{stanleyNormalKVal.toFixed(2)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Slow P</span>
                        <input type="range" class="slider-input"
                               min="0" max="5" step="0.01"
                               value={stanleySlowPVal}
                               onchange={onTuneParam(2)} />
                        <span class="slider-value">{stanleySlowPVal.toFixed(2)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Slow K</span>
                        <input type="range" class="slider-input"
                               min="0" max="5" step="0.01"
                               value={stanleySlowKVal}
                               onchange={onTuneParam(3)} />
                        <span class="slider-value">{stanleySlowKVal.toFixed(2)}</span>
                    </label>
                </div>
                <div class="tune-group">
                    <span class="slider-label">Motor PID</span>
                    <label class="slider-group">
                        <span class="slider-label sub">Kp</span>
                        <input type="range" class="slider-input"
                               min="0" max="10" step="0.01"
                               value={motorPidKpVal}
                               onchange={onTuneParam(4)} />
                        <span class="slider-value">{motorPidKpVal.toFixed(2)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Ki</span>
                        <input type="range" class="slider-input"
                               min="0" max="10" step="0.001"
                               value={motorPidKiVal}
                               onchange={onTuneParam(5)} />
                        <span class="slider-value">{motorPidKiVal.toFixed(3)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Kd</span>
                        <input type="range" class="slider-input"
                               min="0" max="1" step="0.001"
                               value={motorPidKdVal}
                               onchange={onTuneParam(6)} />
                        <span class="slider-value">{motorPidKdVal.toFixed(3)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">LP Tf</span>
                        <input type="range" class="slider-input"
                               min="0" max="1" step="0.001"
                               value={motorPidTfVal}
                               onchange={onTuneParam(7)} />
                        <span class="slider-value">{motorPidTfVal.toFixed(3)}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Ramp</span>
                        <input type="range" class="slider-input"
                               min="0" max="500" step="1"
                               value={motorPidRampVal}
                               onchange={onTuneParam(8)} />
                        <span class="slider-value">{motorPidRampVal}</span>
                    </label>
                    <label class="slider-group">
                        <span class="slider-label sub">Limit</span>
                        <input type="range" class="slider-input"
                               min="0" max="255" step="1"
                               value={motorPidLimitVal}
                               onchange={onTuneParam(9)} />
                        <span class="slider-value">{motorPidLimitVal}</span>
                    </label>
                </div>
            </div>
        {/if}
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
        gap: 8px;
        margin-top: 4px;
        padding-top: 8px;
        border-top: 1px solid #eee;
    }

    .tune-toggle {
        width: 100%;
        padding: 6px 10px;
        font-size: 0.8em;
        font-weight: 500;
        color: #555;
        background: #f4f4f4;
        border: 1px solid #ddd;
        border-radius: 4px;
        cursor: pointer;
        text-align: left;
        transition: background 0.15s;
    }

    .tune-toggle:hover {
        background: #e8e8e8;
    }

    .tune-groups {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .tune-group {
        display: flex;
        flex-direction: column;
        gap: 8px;
        padding: 8px;
        border: 1px solid #eee;
        border-radius: 4px;
        background: #fafafa;
    }

    .tune-group > .slider-label {
        width: auto;
        text-align: left;
        font-weight: 600;
        color: #333;
        margin-bottom: 2px;
    }

    .slider-label.sub {
        width: 55px;
        font-weight: 400;
        color: #666;
    }

    .slider-group {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 0.75em;
        margin-bottom: 4px;
        width: 100%;
    }

    .slider-group:last-child {
        margin-bottom: 0;
    }

    .way-perc-control {
        display: flex;
        align-items: center;
        gap: 4px;
        width: 129px;
        flex-shrink: 0;
        min-width: 0;
    }

    .way-perc-group .step-btn {
        width: 18px;
        height: 18px;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 0;
        font-size: 0.9em;
        font-weight: 600;
        color: #333;
        background: #f4f4f4;
        border: 1px solid #ccc;
        border-radius: 4px;
        cursor: pointer;
        flex-shrink: 0;
        transition: background 0.15s;
    }

    .way-perc-group .step-btn:hover {
        background: #e0e0e0;
    }

    .way-perc-group .step-btn:active {
        background: #c6c6c6;
    }

    .way-perc-control .slider-input {
        flex: 1;
        min-width: 0;
        width: auto;
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
