<script lang="ts">
    import { Tile, Row, Column, Button, Tag } from "carbon-components-svelte";
    import PlayFilledAlt from "carbon-icons-svelte/lib/PlayFilledAlt.svelte";
    import StopFilledAlt from "carbon-icons-svelte/lib/StopFilledAlt.svelte";
    import Home from "carbon-icons-svelte/lib/Home.svelte";
    import SkipForwardFilled from "carbon-icons-svelte/lib/SkipForwardFilled.svelte";
    import { socketStore, socketService } from '../../../stores/socket';
    import { page } from '$app/stores';
    import { browser } from '$app/environment';
    import { Text } from '../../../text';

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
            case 2: return 'blue';  // CHARGE
            case 3: return 'cyan';  // DOCK
            case 0: return 'gray';  // IDLE
            default: return 'gray';
        }
    }

    function jobLabel(job: number | undefined, desc: any): string {
        if (desc == null || job == null) return '—';
        return desc[job] ?? String(job);
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

        <!-- Status-Leiste -->
        <div class="status-bar">
            <Tile class="status-tile">
                <div class="status-main">
                    <div class="status-text">
                        <h1 class="job-label" style:color={jobColor(state?.job) === 'green' ? '#24a148' : jobColor(state?.job) === 'blue' ? '#0f62fe' : jobColor(state?.job) === 'cyan' ? '#1192e8' : '#8d8d8d'}>
                            {jobLabel(state?.job, desc?.job)}
                        </h1>
                        <div class="battery-row">
                            <span class="battery-v">{(state?.battery_voltage ?? 0).toFixed(1)} V</span>
                            <span class="battery-a">({(state?.amps ?? 0).toFixed(1)} A)</span>
                        </div>
                    </div>
                    <div class="status-tags">
                        <Tag type={state?.map_crc != null ? 'green' : 'magenta'}>
                            Karte: {state?.map_crc != null ? 'OK' : '—'}
                        </Tag>
                        {#if sensor?.sonar_obstacle}
                            <Tag type="red">Hindernis</Tag>
                        {/if}
                        {#if sensor?.rain_triggered}
                            <Tag type="blue">Regen</Tag>
                        {/if}
                    </div>
                </div>
            </Tile>
        </div>

        <!-- Kachel-Grid -->
        <div class="metrics-grid">
            <Row narrow>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">⏱️</div>
                        <div class="metric-value">{((stats?.mow ?? 0) / 60).toFixed(1)}</div>
                        <div class="metric-unit">h</div>
                        <div class="metric-label">Mähzeit</div>
                    </Tile>
                </Column>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">📏</div>
                        <div class="metric-value">{(stats?.mow_traveled ?? 0).toFixed(0)}</div>
                        <div class="metric-unit">m</div>
                        <div class="metric-label">Strecke</div>
                    </Tile>
                </Column>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">🚧</div>
                        <div class="metric-value">{stats?.obstacles ?? 0}</div>
                        <div class="metric-unit">×</div>
                        <div class="metric-label">Hindernisse</div>
                    </Tile>
                </Column>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">🌡️</div>
                        <div class="metric-value">{(stats?.temp_max ?? 0).toFixed(0)}</div>
                        <div class="metric-unit">°C</div>
                        <div class="metric-label">Temp. max</div>
                    </Tile>
                </Column>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">📡</div>
                        <div class="metric-value">{state?.position?.solution ?? '—'}</div>
                        <div class="metric-unit">{state?.position?.accuracy ? (state.position.accuracy).toFixed(2) + ' m' : ''}</div>
                        <div class="metric-label">GPS</div>
                    </Tile>
                </Column>
                <Column sm={4} md={4} lg={2}>
                    <Tile class="metric-tile">
                        <div class="metric-icon">💾</div>
                        <div class="metric-value">{(stats?.free_memory ?? 0).toFixed(0)}</div>
                        <div class="metric-unit">B</div>
                        <div class="metric-label">Freier RAM</div>
                    </Tile>
                </Column>
            </Row>
        </div>

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
            <Button kind="primary" icon={PlayFilledAlt} on:click={() => sendCmd('start')}>Start</Button>
            <Button kind="danger" icon={StopFilledAlt} on:click={() => sendCmd('stop')}>Stop</Button>
            <Button kind="secondary" icon={Home} on:click={() => sendCmd('dock')}>Dock</Button>
            <Button kind="ghost" icon={SkipForwardFilled} on:click={() => sendCmd('skipWaypoint')}>Skip</Button>
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
        gap: 12px;
    }

    .status-bar {
        padding: 0 12px;
        padding-top: 12px;
    }

    :global(.status-tile) {
        padding: 1rem !important;
    }

    .status-main {
        display: flex;
        justify-content: space-between;
        align-items: center;
        flex-wrap: wrap;
        gap: 12px;
    }

    .job-label {
        font-size: 1.75rem;
        font-weight: 600;
        margin: 0;
        line-height: 1.2;
    }

    .battery-row {
        display: flex;
        gap: 6px;
        align-items: baseline;
        margin-top: 4px;
    }

    .battery-v {
        font-size: 1.125rem;
        font-weight: 500;
    }

    .battery-a {
        font-size: 0.875rem;
        color: #525252;
    }

    .status-tags {
        display: flex;
        gap: 6px;
        flex-wrap: wrap;
    }

    .metrics-grid {
        padding: 0 12px;
    }

    :global(.metric-tile) {
        padding: 1rem !important;
        text-align: center;
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 2px;
        min-height: 120px;
        justify-content: center;
    }

    .metric-icon {
        font-size: 1.25rem;
        margin-bottom: 2px;
    }

    .metric-value {
        font-size: 1.5rem;
        font-weight: 600;
        line-height: 1.2;
    }

    .metric-unit {
        font-size: 0.75rem;
        color: #525252;
        line-height: 1;
    }

    .metric-label {
        font-size: 0.75rem;
        color: #525252;
        margin-top: 2px;
    }

    .sensor-strip {
        padding: 0 12px;
    }

    .sensor-row {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 16px;
        flex-wrap: wrap;
        padding: 4px 0;
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
        gap: 8px;
        padding: 0 12px;
        padding-bottom: 12px;
        justify-content: center;
        flex-wrap: wrap;
    }
</style>
