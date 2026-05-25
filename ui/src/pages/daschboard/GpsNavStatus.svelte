<script lang="ts">
    import type { NavPvtInfo, NavDopInfo, GpsDetails } from '../../stores/gpsStore';

    interface Props {
        navPvt: NavPvtInfo | null;
        navDop: NavDopInfo | null;
        gpsDetails: GpsDetails | null;
    }

    let { navPvt, navDop, gpsDetails }: Props = $props();

    function fixTypeName(fixType: number): string {
        switch (fixType) {
            case 0: return 'No fix';
            case 1: return 'DR only';
            case 2: return '2D';
            case 3: return '3D';
            case 4: return 'GNSS+DR';
            case 5: return 'Time only';
            default: return `Code ${fixType}`;
        }
    }

    function carrSolnName(carr: number): string {
        switch (carr) {
            case 0: return 'None';
            case 1: return 'Float';
            case 2: return 'Fixed';
            default: return `Code ${carr}`;
        }
    }

    let fixColor = $derived.by(() => {
        if (!navPvt) return 'nofix';
        if (navPvt.fixType === 0) return 'nofix';
        if (navPvt.carrSoln === 2) return 'fixed';
        if (navPvt.carrSoln === 1) return 'float';
        if (navPvt.fixType >= 3) return 'fix3d';
        return 'fix2d';
    });
</script>

<div class="navstatus-panel">
    <div class="uc-panel-header">
        <span class="uc-panel-title">Navigation Status</span>
    </div>
    <div class="navstatus-body">
        {#if navPvt}
            <div class="navstatus-badge {fixColor}">
                {fixTypeName(navPvt.fixType)}
                {#if navPvt.carrSoln > 0}
                    <span class="navstatus-carr">({carrSolnName(navPvt.carrSoln)})</span>
                {/if}
            </div>
        {:else if gpsDetails}
            <div class="navstatus-badge {gpsDetails.solution === 2 ? 'fixed' : gpsDetails.solution === 1 ? 'float' : 'nofix'}">
                {gpsDetails.solution === 2 ? 'Fixed' : gpsDetails.solution === 1 ? 'Float' : 'Invalid'}
            </div>
        {:else}
            <div class="navstatus-badge nofix">No data</div>
        {/if}

        <div class="navstatus-grid">
            {#if navPvt}
                <div class="navstatus-item">
                    <span class="navstatus-key">Longitude</span>
                    <span class="navstatus-val">{(navPvt.lon ?? 0).toFixed(7)}°</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">Latitude</span>
                    <span class="navstatus-val">{(navPvt.lat ?? 0).toFixed(7)}°</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">Altitude (Ell)</span>
                    <span class="navstatus-val">{(navPvt.height ?? 0).toFixed(3)} m</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">Altitude (MSL)</span>
                    <span class="navstatus-val">{(navPvt.hMSL ?? 0).toFixed(3)} m</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">Ground Speed</span>
                    <span class="navstatus-val">{(navPvt.gSpeed ?? 0).toFixed(2)} m/s</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">Heading</span>
                    <span class="navstatus-val">{(navPvt.heading ?? 0).toFixed(2)}°</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">3D Acc</span>
                    <span class="navstatus-val {(navPvt.hAcc ?? 999) < 0.1 ? 'good' : (navPvt.hAcc ?? 999) < 1.0 ? 'ok' : 'warn'}">{(navPvt.hAcc ?? 0).toFixed(2)} m</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">V-Acc</span>
                    <span class="navstatus-val">{(navPvt.vAcc ?? 0).toFixed(2)} m</span>
                </div>
            {/if}

            {#if navDop}
                <div class="navstatus-item">
                    <span class="navstatus-key">PDOP</span>
                    <span class="navstatus-val {(navDop.pDOP ?? 999) < 2 ? 'good' : (navDop.pDOP ?? 999) < 5 ? 'ok' : 'warn'}">{(navDop.pDOP ?? 0).toFixed(1)}</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">HDOP</span>
                    <span class="navstatus-val {(navDop.hDOP ?? 999) < 1 ? 'good' : (navDop.hDOP ?? 999) < 2 ? 'ok' : 'warn'}">{(navDop.hDOP ?? 0).toFixed(1)}</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">VDOP</span>
                    <span class="navstatus-val">{(navDop.vDOP ?? 0).toFixed(1)}</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">GDOP</span>
                    <span class="navstatus-val">{(navDop.gDOP ?? 0).toFixed(1)}</span>
                </div>
            {/if}

            {#if gpsDetails}
                {#if !navPvt}
                <div class="navstatus-item">
                    <span class="navstatus-key">H-Accuracy</span>
                    <span class="navstatus-val {(gpsDetails.hAccuracy ?? 999) < 0.1 ? 'good' : (gpsDetails.hAccuracy ?? 999) < 1.0 ? 'ok' : 'warn'}">{(gpsDetails.hAccuracy ?? 0).toFixed(2)} m</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">V-Accuracy</span>
                    <span class="navstatus-val">{(gpsDetails.vAccuracy ?? 0).toFixed(2)} m</span>
                </div>
                {/if}
                <div class="navstatus-item">
                    <span class="navstatus-key">Satellites</span>
                    <span class="navstatus-val">{gpsDetails.numSV} <span class="navstatus-sub">/ {gpsDetails.satellites.length}</span></span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">DGPS Sats</span>
                    <span class="navstatus-val">{gpsDetails.numSVdgps}</span>
                </div>
                <div class="navstatus-item">
                    <span class="navstatus-key">DGPS Age</span>
                    <span class="navstatus-val">{gpsDetails.dgpsAge} ms</span>
                </div>
            {/if}
        </div>
    </div>
</div>

<style lang="scss">
    .navstatus-panel {
        background: white;
        border-bottom: 1px solid #ddd;
        min-width: 220px;
    }
    .navstatus-body {
        padding: 10px;
    }
    .navstatus-badge {
        padding: 8px 12px;
        border-radius: 6px;
        text-align: center;
        font-size: 1.1em;
        font-weight: bold;
        margin-bottom: 10px;
        color: white;
    }
    .navstatus-badge.fixed {
        background: #1e8e3e;
    }
    .navstatus-badge.float {
        background: #ffab00;
        color: #333;
    }
    .navstatus-badge.fix3d {
        background: #1565c0;
    }
    .navstatus-badge.fix2d {
        background: #b06000;
    }
    .navstatus-badge.nofix {
        background: #c5221f;
    }
    .navstatus-carr {
        font-size: 0.85em;
        opacity: 0.9;
    }
    .navstatus-grid {
        display: grid;
        grid-template-columns: 1fr;
        gap: 4px;
    }
    .navstatus-item {
        display: flex;
        justify-content: space-between;
        padding: 4px 6px;
        background: #f8f8f8;
        border-radius: 3px;
        font-size: 0.8em;
    }
    .navstatus-key {
        color: #888;
    }
    .navstatus-val {
        color: #333;
        font-family: monospace;
        font-weight: 600;
    }
    .navstatus-val.good {
        color: #1e8e3e;
    }
    .navstatus-val.ok {
        color: #ffab00;
    }
    .navstatus-val.warn {
        color: #c5221f;
    }
    .navstatus-sub {
        font-weight: normal;
        color: #888;
        font-size: 0.9em;
    }

    .uc-panel-header {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 8px 12px;
        background: #f4f4f4;
        border-bottom: 1px solid #ddd;
        font-size: 0.9em;
    }
    .uc-panel-title {
        font-weight: 600;
        color: #333;
    }
    .uc-panel-sub {
        font-size: 0.8em;
        color: #888;
    }
</style>
