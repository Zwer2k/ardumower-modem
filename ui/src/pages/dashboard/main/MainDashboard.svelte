<script lang="ts">
    import { Tile, Row, Column, Button, Tag } from "carbon-components-svelte";
    import PlayFilledAlt from "carbon-icons-svelte/lib/PlayFilledAlt.svelte";
    import StopFilledAlt from "carbon-icons-svelte/lib/StopFilledAlt.svelte";
    import Home from "carbon-icons-svelte/lib/Home.svelte";
    import SkipForwardFilled from "carbon-icons-svelte/lib/SkipForwardFilled.svelte";
    import { socketStore, socketService } from '../../../stores/socket';
    import { page } from '$app/stores';
    import { browser } from '$app/environment';
    import MiniMap from './MiniMap.svelte';
    import { MapStore } from '../../../map/service';

    $effect(() => {
        if (!browser) return;
        const dashboard = $page.url?.searchParams?.get('dashboard');
        if (dashboard === 'main') {
            socketService.requestSensorSummary();
        } else {
            socketService.stopSensorSummary();
        }
    });

    function jobColor(job: number | undefined): string {
        if (job === undefined) return 'gray';
        switch (job) {
            case 1: return 'green';   // MOW
            case 2: return 'blue';    // CHARGE
            case 4: return 'cyan';    // DOCK
            case 3: return 'red';     // ERROR
            case 0: return 'gray';    // IDLE
            default: return 'gray';
        }
    }

    function jobIcon(job: number | undefined): string {
        switch (job) {
            case 1: return '🟢';  // MOW
            case 2: return '🔌';  // CHARGE
            case 4: return '🏠';  // DOCK
            case 3: return '⚠️';  // ERROR
            case 0: return '💤';  // IDLE
            default: return '❓';
        }
    }

    function jobLabel(job: number | undefined, desc: any): string {
        if (desc == null || job == null) return '—';
        return desc[job] ?? String(job);
    }

    function solutionLabel(solution: number | undefined): string {
        switch (solution) {
            case 0: return 'None';
            case 1: return '3D';
            case 2: return 'RTK float';
            case 3: return 'RTK fixed';
            default: return '—';
        }
    }

    function sendCmd(action: string) {
        socketService.sendCommand({ action });
    }
</script>

<div class="dashboard-content">
    {#if browser && $socketStore.valueDescriptions != null}
        {@const state = $socketStore.state}
        {@const stats = $socketStore.stats}
        {@const sensor = $socketStore.sensorSummary}
        {@const desc = $socketStore.valueDescriptions}
        {@const pos = state?.position}
        {@const totalPoints = ($MapStore?.map?.waypoints?.points?.length ?? 0)}
        {@const currentPoint = (pos?.mow_point_index ?? -1) >= 0 ? pos.mow_point_index + 1 : 0}

        <Row narrow class="metrics-row">
            <Column sm={4} md={8} lg={5} class="metrics-col">
                <div class="top-grid">
                    <Tile class="status-tile compact">
                        <div class="status-icon" style:color={jobColor(state?.job) === 'green' ? '#24a148' : jobColor(state?.job) === 'blue' ? '#0f62fe' : jobColor(state?.job) === 'cyan' ? '#1192e8' : jobColor(state?.job) === 'red' ? '#da1e28' : '#8d8d8d'}>{jobIcon(state?.job)}</div>
                        <div class="status-value" style:color={jobColor(state?.job) === 'green' ? '#24a148' : jobColor(state?.job) === 'blue' ? '#0f62fe' : jobColor(state?.job) === 'cyan' ? '#1192e8' : jobColor(state?.job) === 'red' ? '#da1e28' : '#8d8d8d'}>{jobLabel(state?.job, desc?.job)}</div>
                        <div class="status-label">Status</div>
                    </Tile>
                    <Tile class="battery-tile compact">
                        <div class="battery-icon">🔋</div>
                        <div class="metric-value">{(state?.battery_voltage ?? 0).toFixed(1)}</div>
                        <div class="metric-unit">V</div>
                        <div class="metric-divider"></div>
                        <div class="metric-icon">⚡</div>
                        <div class="metric-value">{Math.abs(state?.amps ?? 0).toFixed(1)}</div>
                        <div class="metric-unit">A</div>
                        <div class="battery-label">Akku</div>
                    </Tile>
                </div>
                <div class="metrics-grid">
                    <Tile class="metric-tile compact">
                        <div class="metric-icon">⏱️</div>
                        <div class="metric-value">{((stats?.mow ?? 0) / 60).toFixed(1)}</div>
                        <div class="metric-unit">h</div>
                        <div class="metric-divider"></div>
                        <div class="metric-icon">📏</div>
                        <div class="metric-value">{(stats?.mow_traveled ?? 0).toFixed(0)}</div>
                        <div class="metric-unit">m</div>
                        <div class="metric-label">Mähzeit / Strecke</div>
                    </Tile>
                    <Tile class="metric-tile compact">
                        <div class="metric-icon">🌡️</div>
                        {#if (stats?.temp_max ?? -999) > -100 && (stats?.temp_max ?? 999) < 200}
                            <div class="metric-value">{(stats?.temp_max ?? 0).toFixed(0)}</div>
                            <div class="metric-unit">°C</div>
                        {:else}
                            <div class="metric-value">—</div>
                        {/if}
                        <div class="metric-divider"></div>
                        <div class="metric-icon">🧠</div>
                        <div class="metric-value">{((stats?.free_memory ?? 0) / 1024).toFixed(1)}</div>
                        <div class="metric-unit">kB</div>
                        <div class="metric-label">Temp / RAM</div>
                    </Tile>
                    <Tile class="metric-tile compact gps-position">
                        <div class="metric-icon">📍</div>
                        <div class="metric-stack">
                            <span class="stack-row"><span class="stack-label">E</span>{(pos?.x ?? 0).toFixed(2)}</span>
                            <span class="stack-row"><span class="stack-label">N</span>{(pos?.y ?? 0).toFixed(2)}</span>
                            <span class="stack-row"><span class="stack-label">Δ</span>{pos?.delta != null ? ((pos.delta * 180) / Math.PI).toFixed(0) + '°' : '—'}</span>
                        </div>
                        <div class="metric-label">Position</div>
                    </Tile>
                    <Tile class="metric-tile compact gps-quality">
                        <div class="metric-icon">📡</div>
                        <div class="metric-stack">
                            <span class="stack-row"><span class="stack-label">Sol</span>{solutionLabel(pos?.solution)}</span>
                            <span class="stack-row"><span class="stack-label">Acc</span>{(pos?.accuracy ?? 0).toFixed(2)} m</span>
                            <span class="stack-row"><span class="stack-label">Sat</span>{pos?.visible_satellites ?? 0} ({pos?.visible_satellites_dgps ?? 0})</span>
                        </div>
                        <div class="metric-label">GPS</div>
                    </Tile>
                    <Tile class="metric-tile compact">
                        <div class="metric-icon">🚧</div>
                        <div class="metric-value">{stats?.obstacles ?? 0}</div>
                        <div class="metric-unit">×</div>
                        <div class="metric-divider"></div>
                        <div class="metric-icon">🛤️</div>
                        <div class="metric-value">{currentPoint}</div>
                        <span class="metric-unit">/{totalPoints}</span>
                        <div class="metric-label">Hindernisse / Punkte</div>
                    </Tile>
                </div>
            </Column><!--
            --><Column sm={4} md={8} lg={6} class="map-col">
                <Tile class="map-tile">
                    <MiniMap mapCrc={state?.map_crc} mowPointIndex={pos?.mow_point_index ?? -1} />
                </Tile>
            </Column>
        </Row>

        <!-- Sensor-Strip -->
        {#if sensor}
            <div class="sensor-strip">
                <Tile>
                    <div class="sensor-row">
                        <div class="sensor-group">
                            <span class="sensor-title">Sonar</span>
                            <div class="sensor-dots">
                                <div class="sensor-dot" class:alert={sensor.sonar_left > 0 && sensor.sonar_left < 20} title="Links: {sensor.sonar_left} cm">
                                    L
                                </div>
                                <div class="sensor-dot" class:alert={sensor.sonar_center > 0 && sensor.sonar_center < 20} title="Mitte: {sensor.sonar_center} cm">
                                    M
                                </div>
                                <div class="sensor-dot" class:alert={sensor.sonar_right > 0 && sensor.sonar_right < 20} title="Rechts: {sensor.sonar_right} cm">
                                    R
                                </div>
                            </div>
                        </div>
                        <div class="sensor-divider"></div>
                        <div class="sensor-group">
                            <span class="sensor-title">Bumper</span>
                            <div class="sensor-dots">
                                <div class="sensor-dot" class:alert={sensor.bumper_left} title="Links">L</div>
                                <div class="sensor-dot" class:alert={sensor.bumper_right} title="Rechts">R</div>
                            </div>
                        </div>
                        <div class="sensor-divider"></div>
                        <div class="sensor-group">
                            <span class="sensor-title">Status</span>
                            <div class="sensor-dots">
                                <div class="sensor-dot" class:alert={sensor.lift_triggered} title="Hebung">🏗️</div>
                                <div class="sensor-dot" class:alert={sensor.rain_triggered} title="Regen">🌧️</div>
                                <div class="sensor-dot" class:alert={sensor.lidar_obstacle} title="Lidar">📡</div>
                            </div>
                        </div>
                    </div>
                </Tile>
            </div>
        {/if}

        <!-- Schnellaktionen -->
        <div class="quick-actions">
            <Button kind="primary" icon={PlayFilledAlt} size="small" class="action-start" on:click={() => sendCmd('start')} />
            <Button kind="danger" icon={StopFilledAlt} size="small" class="action-stop" on:click={() => sendCmd('stop')} />
            <Button kind="secondary" icon={Home} size="small" class="action-dock" on:click={() => sendCmd('dock')} />
            <Button kind="tertiary" icon={SkipForwardFilled} size="small" class="action-skip" on:click={() => sendCmd('skipWaypoint')} />
        </div>
    {/if}
</div>

<style lang="scss">
    .dashboard-content {
        width: 100%;
        height: 100vh;
        min-height: 100vh;
        padding: 0;
        margin: 0;
        display: flex;
        flex-direction: column;
        padding-top: 48px;
        gap: 6px;
        box-sizing: border-box;
    }

    .top-grid {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 6px;
        margin-bottom: 6px;
    }

    :global(.status-tile) {
        padding: 0.375rem 0.5rem !important;
        min-height: 32px;
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 4px;
        justify-content: flex-start;
        margin-bottom: 0;
    }

    .status-icon {
        font-size: 1rem;
        line-height: 1;
    }

    .status-value {
        font-size: 1rem;
        font-weight: 600;
        line-height: 1;
    }

    .status-label {
        font-size: 0.75rem;
        color: #525252;
        margin-left: auto;
        white-space: nowrap;
        line-height: 1;
    }

    :global(.battery-tile) {
        padding: 0.375rem 0.5rem !important;
        min-height: 32px;
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 4px;
        justify-content: flex-start;
        margin-bottom: 0;
    }

    .battery-icon {
        font-size: 1rem;
        line-height: 1;
    }

    .battery-label {
        font-size: 0.75rem;
        color: #525252;
        margin-left: auto;
        white-space: nowrap;
        line-height: 1;
    }

    @media (max-width: 960px) {
        :global(.status-tile),
        :global(.battery-tile) {
            padding: 0.25rem 0.5rem !important;
            min-height: 28px;
            gap: 3px;
            justify-content: center;
        }

        .status-value,
        .battery-label {
            display: none;
        }
    }

    :global(.metrics-row) {
        margin-left: 0 !important;
        margin-right: 0 !important;
        flex-wrap: nowrap !important;
        flex: 1 1 auto !important;
        min-height: 0 !important;
        align-items: stretch !important;
    }

    :global(.metrics-row .metrics-col) {
        flex: 0 0 calc(31.25% - 12px) !important;
        max-width: calc(31.25% - 12px) !important;
        padding-left: 12px;
        padding-right: 8px;
        display: flex;
        flex-direction: column;
    }

    :global(.metrics-row .map-col) {
        flex: 0 0 calc(68.75% - 4px) !important;
        max-width: calc(68.75% - 4px) !important;
        padding-left: 0;
        padding-right: 12px;
        display: flex;
        flex-direction: column;
        height: 100%;
    }

    @media (max-width: 960px) {
        :global(.metrics-row) {
            flex-direction: column !important;
            flex-wrap: nowrap !important;
            flex: 1 1 auto !important;
            min-height: 0 !important;
        }

        :global(.metrics-row .metrics-col),
        :global(.metrics-row .map-col) {
            flex: 0 0 auto !important;
            max-width: 100% !important;
            padding-left: 8px;
            padding-right: 8px;
        }

        :global(.metrics-row .map-col) {
            flex: 1 1 auto !important;
            min-height: 0 !important;
            display: flex;
            flex-direction: column;
        }

        :global(.map-tile) {
            height: auto !important;
            min-height: 140px !important;
            max-height: none !important;
            flex: 1 1 auto !important;
            overflow: hidden !important;
        }

        :global(.map-tile > div) {
            flex: 1 1 auto !important;
            min-height: 0 !important;
            height: auto !important;
        }

        .sensor-strip {
            flex: 0 0 auto !important;
            padding: 0 8px;
        }

        :global(.sensor-strip .bx--tile) {
            padding: 0 !important;
            min-height: auto !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
        }

        .sensor-row {
            gap: 8px;
            padding: 0 8px;
        }

        .sensor-group {
            gap: 2px;
        }

        .sensor-dot {
            width: 24px;
            height: 24px;
            font-size: 0.5rem;
        }

        .sensor-divider {
            height: 24px;
        }

        .sensor-title {
            display: none;
        }
    }

    .metrics-grid {
        display: flex;
        flex-direction: column;
        gap: 6px;
    }

    @media (max-width: 960px) {
        .metrics-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
        }
    }

    :global(.metric-tile) {
        padding: 0.375rem 0.75rem !important;
        text-align: left;
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 6px;
        min-height: 32px;
        justify-content: flex-start;
    }

    @media (min-width: 961px) {
        :global(.map-tile) {
            padding: 0 !important;
            flex: 1 1 auto;
            height: 100%;
            min-height: 0;
            overflow: hidden;
            display: flex;
            flex-direction: column;
        }
    }

    :global(.map-tile) {
        padding: 0 !important;
        overflow: hidden;
        display: flex;
        flex-direction: column;
    }

    :global(.map-tile > div) {
        flex: 1 1 auto;
        min-height: 0;
        display: flex;
        flex-direction: column;
    }

    .metric-icon {
        font-size: 1rem;
        margin-right: 2px;
    }

    .metric-value {
        font-size: 1rem;
        font-weight: 600;
        line-height: 1;
    }

    .metric-unit {
        font-size: 0.75rem;
        color: #525252;
        line-height: 1;
    }

    .metric-divider {
        width: 1px;
        height: 18px;
        background: #e0e0e0;
        margin: 0 2px;
    }

    .metric-stack {
        display: flex;
        flex-direction: column;
        gap: 1px;
        font-size: 0.75rem;
        line-height: 1.1;
    }

    .stack-row {
        display: flex;
        align-items: baseline;
        gap: 4px;
    }

    .stack-label {
        font-size: 0.625rem;
        color: #8d8d8d;
        text-transform: uppercase;
        min-width: 1.25rem;
    }

    .metric-label {
        font-size: 0.75rem;
        color: #525252;
        margin-left: auto;
        white-space: nowrap;
        line-height: 1;
    }

    @media (max-width: 960px) {
        :global(.metric-tile) {
            justify-content: center;
            padding: 0.25rem 0.5rem !important;
            min-height: 28px;
        }

        .metric-label {
            display: none;
        }

        .metric-value {
            font-size: 1rem;
        }

        .metric-stack {
            font-size: 0.7rem;
        }

        .stack-label {
            font-size: 0.6rem;
        }

        .top-grid {
            grid-template-columns: 1fr 1fr;
        }

        :global(.status-tile),
        :global(.battery-tile) {
            padding: 0.25rem 0.5rem !important;
            min-height: 28px;
            gap: 3px;
        }

        .status-label,
        .battery-label {
            display: none;
        }

        :global(.map-tile) {
            height: auto !important;
            min-height: 140px !important;
            max-height: none !important;
            flex: 1 1 auto !important;
            overflow: hidden !important;
        }

        :global(.map-tile > div) {
            flex: 1 1 auto !important;
            min-height: 0 !important;
            height: auto !important;
        }
    }

    @media (min-width: 673px) {
        .metric-label {
            margin-left: auto;
        }
    }

    .sensor-strip {
        padding: 0 8px;
        flex: 0 0 auto;
    }

    :global(.sensor-strip .bx--tile) {
        padding: 0 !important;
        min-height: 0 !important;
        height: auto !important;
        display: flex !important;
        align-items: center !important;
        justify-content: center !important;
    }

    .sensor-row {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 16px;
        flex-wrap: wrap;
        padding: 0;
    }

    .sensor-group {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 4px;
    }

    .sensor-title {
        font-size: 0.625rem;
        text-transform: uppercase;
        letter-spacing: 0.05em;
        color: #525252;
        font-weight: 500;
    }

    .sensor-dots {
        display: flex;
        gap: 6px;
    }

    .sensor-dot {
        width: 28px;
        height: 28px;
        border-radius: 50%;
        background: #e8e8e8;
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 0.625rem;
        font-weight: 600;
        color: #525252;
        transition: background 0.2s, color 0.2s;
    }

    .sensor-dot.alert {
        background: #da1e28;
        color: white;
    }

    .sensor-divider {
        width: 1px;
        height: 32px;
        background: #e0e0e0;
    }

    .quick-actions {
        display: flex;
        gap: 6px;
        padding: 0 8px;
        padding-bottom: 4px;
        justify-content: center;
        flex-wrap: wrap;
        flex: 0 0 auto;
        margin-top: auto;
    }

    :global(.quick-actions .bx--btn) {
        min-height: 32px;
        padding: 0;
        width: 36px;
        justify-content: center;
    }

    :global(.quick-actions .bx--btn__icon) {
        margin: 0 !important;
    }

    @media (min-width: 961px) {
        :global(.quick-actions .bx--btn) {
            width: auto;
            padding: 0 12px;
            gap: 8px;
        }

        :global(.action-start .bx--btn__icon)::after {
            content: 'Start';
            margin-left: 8px;
            font-size: 0.875rem;
            white-space: nowrap;
        }

        :global(.action-stop .bx--btn__icon)::after {
            content: 'Stop';
            margin-left: 8px;
            font-size: 0.875rem;
            white-space: nowrap;
        }

        :global(.action-dock .bx--btn__icon)::after {
            content: 'Dock';
            margin-left: 8px;
            font-size: 0.875rem;
            white-space: nowrap;
        }

        :global(.action-skip .bx--btn__icon)::after {
            content: 'Skip';
            margin-left: 8px;
            font-size: 0.875rem;
            white-space: nowrap;
        }
    }
</style>
