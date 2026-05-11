<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import { page } from '$app/stores';
    import { browser } from '$app/environment';
    import { socketStore, socketService } from '../../stores/socket';

    function gnssName(gnssId: number): string {
        switch (gnssId) {
            case 0: return "GPS";
            case 1: return "SBAS";
            case 2: return "Galileo";
            case 3: return "BeiDou";
            case 4: return "IMES";
            case 5: return "QZSS";
            case 6: return "GLONASS";
            default: return `GNSS-${gnssId}`;
        }
    }

    function qualityName(qi: number): string {
        switch (qi) {
            case 0: return "no signal";
            case 1: return "searching";
            case 2: return "acquired";
            case 3: return "locked";
            case 4: return "locked+time";
            default: return `q${qi}`;
        }
    }

    function solutionName(sol: number): string {
        switch (sol) {
            case 0: return "Invalid";
            case 1: return "Float";
            case 2: return "Fixed";
            default: return `Sol-${sol}`;
        }
    }

    // Reagiere auf Sichtbarkeit (Dashboard ist immer im DOM, nur CSS hidden)
    $effect(() => {
        if (!browser) return;
        const dashboard = $page.url?.searchParams?.get('dashboard');
        if (dashboard === 'gps') {
            console.log('[GpsDashboard] Activating GPS polling');
            socketService.requestGpsDetails();
        } else {
            console.log('[GpsDashboard] Deactivating GPS polling');
            socketService.stopGpsDetails();
        }
    });

    let satellites = $derived($socketStore.gpsDetails?.satellites ?? []);
    let sortedSats = $derived([...satellites].sort((a, b) => b.cno - a.cno));
    let usedCount = $derived(satellites.filter(s => s.prUsed).length);
    let dgpsCount = $derived(satellites.filter(s => s.crCorrUsed).length);
    let lastUpdate = $derived($socketStore.gpsDetails?.timestamp ?? 0);
    let isStale = $derived(lastUpdate > 0 && (Date.now() - lastUpdate) > 10000);
</script>

<div class="dashboard-content">
    {#if $socketStore.gpsDetails != null}
        {@const d = $socketStore.gpsDetails}
        <div class="gps-stats">
            <div class="stat-card">
                <div class="stat-label">Solution</div>
                <div class="stat-value" class:fix={d.solution === 2} class:float={d.solution === 1}>
                    {solutionName(d.solution)}
                </div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Satellites</div>
                <div class="stat-value">{d.numSV} <span class="stat-small">({usedCount} used, {dgpsCount} dgps)</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">H-Accuracy</div>
                <div class="stat-value">{d.hAccuracy.toFixed(3)} m</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">V-Accuracy</div>
                <div class="stat-value">{d.vAccuracy.toFixed(3)} m</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">DGPs Age</div>
                <div class="stat-value">{d.dgpsAge} ms</div>
            </div>
        </div>

        <div class="cn0-section">
            <h4>Signal Strength (C/N₀)</h4>
            <div class="cn0-bars">
                {#each sortedSats as sat}
                    {@const gnss = gnssName(sat.gnssId)}
                    {@const pct = Math.min(100, (sat.cno / 55) * 100)}
                    <div class="cn0-row">
                        <div class="cn0-label">{gnss} {sat.svId}</div>
                        <div class="cn0-bar-bg">
                            <div class="cn0-bar-fill" style="width: {pct}%; background: {sat.cno >= 40 ? '#1e8e3e' : sat.cno >= 30 ? '#b06000' : '#c5221f'}"></div>
                        </div>
                        <div class="cn0-value">{sat.cno} dB-Hz</div>
                        {#if sat.prUsed}<span class="badge used">U</span>{/if}
                        {#if sat.crCorrUsed}<span class="badge dgps">D</span>{/if}
                    </div>
                {/each}
            </div>
        </div>

        <div class="sat-table-section">
            <h4>Satellite Details</h4>
            <div class="sat-table-wrapper">
                <table class="sat-table">
                    <thead>
                        <tr>
                            <th>GNSS</th>
                            <th>SV</th>
                            <th>Sig</th>
                            <th>C/N₀</th>
                            <th>Quality</th>
                            <th>PR Res</th>
                            <th>Used</th>
                            <th>DGPS</th>
                        </tr>
                    </thead>
                    <tbody>
                        {#each sortedSats as sat}
                            <tr class:used-row={sat.prUsed} class:dgps-row={sat.crCorrUsed}>
                                <td>{gnssName(sat.gnssId)}</td>
                                <td>{sat.svId}</td>
                                <td>{sat.sigId}</td>
                                <td class:good={sat.cno >= 40} class:weak={sat.cno < 30}>{sat.cno}</td>
                                <td>{qualityName(sat.qualityInd)}</td>
                                <td>{sat.prRes.toFixed(1)} m</td>
                                <td>{sat.prUsed ? "Yes" : "No"}</td>
                                <td>{sat.crCorrUsed ? "Yes" : "No"}</td>
                            </tr>
                        {/each}
                    </tbody>
                </table>
            </div>
        </div>
    {:else}
        <div class="no-data">
            {#if !$socketStore.connected}
                <div class="no-data-title">Not connected</div>
                <div class="no-data-text">WebSocket connection lost. Trying to reconnect...</div>
            {:else}
                <div class="no-data-title">No GPS data</div>
                <div class="no-data-text">
                    GPS receiver is not responding or has no signal.<br>
                    Make sure the rover is powered on and has GPS reception.
                </div>
                <div class="no-data-hint">
                    Last known state: {$socketStore.state ? ($socketStore.state.position.solution === 0 ? 'Invalid' : $socketStore.state.position.solution === 1 ? 'Float' : 'Fixed') : 'Unknown'}
                </div>
            {/if}
        </div>
    {/if}
</div>

<style lang="scss">
    .dashboard-content {
        width: 100%;
        height: 100vh;
        padding: 0;
        margin: 0;
        display: flex;
        flex-direction: column;
        overflow-y: auto;
        padding-top: 48px;
        box-sizing: border-box;
    }

    .gps-stats {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
        gap: 8px;
        padding: 8px;
        background: #f8f8f8;
        border-bottom: 1px solid #ddd;
    }

    .stat-card {
        background: white;
        border: 1px solid #ddd;
        border-radius: 4px;
        padding: 8px 12px;
        text-align: center;
    }

    .stat-label {
        font-size: 0.75em;
        color: #666;
        margin-bottom: 4px;
    }

    .stat-value {
        font-size: 1.1em;
        font-weight: bold;
        color: #333;
    }

    .stat-value.fix { color: #1e8e3e; }
    .stat-value.float { color: #b06000; }

    .stat-small {
        font-size: 0.75em;
        font-weight: normal;
        color: #666;
    }

    .cn0-section {
        padding: 12px;
        border-bottom: 1px solid #eee;
    }

    h4 {
        margin: 0 0 8px 0;
        font-size: 0.95em;
        color: #333;
    }

    .cn0-bars {
        display: flex;
        flex-direction: column;
        gap: 4px;
    }

    .cn0-row {
        display: grid;
        grid-template-columns: 70px 1fr 80px 24px 24px;
        gap: 6px;
        align-items: center;
        font-size: 0.85em;
    }

    .cn0-label {
        font-family: monospace;
        font-size: 0.85em;
    }

    .cn0-bar-bg {
        height: 14px;
        background: #e8e8e8;
        border-radius: 3px;
        overflow: hidden;
    }

    .cn0-bar-fill {
        height: 100%;
        border-radius: 3px;
        transition: width 0.3s ease;
    }

    .cn0-value {
        font-family: monospace;
        font-size: 0.85em;
        text-align: right;
    }

    .badge {
        font-size: 0.65em;
        padding: 1px 3px;
        border-radius: 3px;
        font-weight: bold;
        text-align: center;
    }

    .badge.used {
        background: #e6f4ea;
        color: #1e8e3e;
    }

    .badge.dgps {
        background: #e0f7fa;
        color: #006064;
    }

    .sat-table-section {
        padding: 12px;
        flex: 1;
        overflow: hidden;
    }

    .sat-table-wrapper {
        overflow-x: auto;
    }

    .sat-table {
        width: 100%;
        border-collapse: collapse;
        font-size: 0.85em;
    }

    .sat-table th {
        text-align: left;
        padding: 6px 8px;
        border-bottom: 2px solid #ddd;
        background: #f4f4f4;
        font-weight: 600;
        white-space: nowrap;
    }

    .sat-table td {
        padding: 5px 8px;
        border-bottom: 1px solid #eee;
        white-space: nowrap;
    }

    .sat-table tbody tr:hover {
        background: #f8f8f8;
    }

    .used-row {
        background: #f0f8f0;
    }

    .dgps-row {
        background: #f0f4f8;
    }

    .good {
        color: #1e8e3e;
        font-weight: bold;
    }

    .weak {
        color: #c5221f;
    }

    .no-data {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100%;
        color: #888;
        text-align: center;
        padding: 2rem;
    }

    .no-data-title {
        font-size: 1.3em;
        font-weight: bold;
        color: #555;
        margin-bottom: 0.5rem;
    }

    .no-data-text {
        font-size: 0.95em;
        margin-bottom: 1rem;
        line-height: 1.5;
    }

    .no-data-hint {
        font-size: 0.85em;
        color: #666;
        background: #f4f4f4;
        padding: 0.5rem 1rem;
        border-radius: 4px;
    }
</style>
