<script lang="ts">
    import type { PositionSample } from '../../stores/gpsStore';

    interface Props {
        positionHistory: PositionSample[];
        refLat: number | null;
        refLon: number | null;
    }

    let { positionHistory, refLat, refLon }: Props = $props();

    const SIZE = 280;
    const CX = SIZE / 2;
    const CY = SIZE / 2;
    const MAX_R = SIZE / 2 - 25;

    // Earth radius in meters
    const EARTH_R = 6371000;

    function latLonToMeters(lat: number, lon: number, refLatVal: number, refLonVal: number) {
        const dLat = ((lat - refLatVal) * Math.PI) / 180;
        const dLon = ((lon - refLonVal) * Math.PI) / 180;
        const avgLat = ((lat + refLatVal) / 2 * Math.PI) / 180;
        const x = dLon * EARTH_R * Math.cos(avgLat); // East-West
        const y = dLat * EARTH_R; // North-South
        return { x, y };
    }

    let points = $derived(() => {
        if (refLat === null || refLon === null || positionHistory.length < 2) return [];
        const pts: { x: number; y: number }[] = [];
        for (const p of positionHistory) {
            const m = latLonToMeters(p.lat, p.lon, refLat, refLon);
            pts.push(m);
        }
        return pts;
    });

    let maxDist = $derived(() => {
        const pts = points();
        if (pts.length === 0) return 1.0;
        let max = 0;
        for (const p of pts) {
            const d = Math.sqrt(p.x * p.x + p.y * p.y);
            if (d > max) max = d;
        }
        return Math.max(max, 0.5);
    });

    let scale = $derived(() => {
        const md = maxDist();
        // Round up to nice number
        const step = md <= 0.5 ? 0.1 : md <= 1.0 ? 0.2 : md <= 2.0 ? 0.5 : md <= 5.0 ? 1.0 : 2.0;
        const maxRing = Math.ceil(md / step) * step;
        return MAX_R / maxRing;
    });

    let maxRing = $derived(() => {
        const s = scale();
        return MAX_R / s;
    });

    let ringSteps = $derived(() => {
        const mr = maxRing();
        const steps: number[] = [];
        const step = mr <= 1.0 ? 0.2 : mr <= 2.0 ? 0.5 : 1.0;
        for (let r = step; r <= mr + 0.001; r += step) {
            steps.push(parseFloat(r.toFixed(2)));
        }
        return steps;
    });

    let svgPoints = $derived(() => {
        const pts = points();
        const s = scale();
        return pts.map((p) => ({
            x: CX + p.x * s,
            y: CY - p.y * s,
        }));
    });

    let pathD = $derived(() => {
        const sp = svgPoints();
        if (sp.length < 2) return "";
        return sp.map((p, i) => `${i === 0 ? "M" : "L"}${p.x},${p.y}`).join(" ");
    });

    let lastPoint = $derived(() => {
        const sp = svgPoints();
        return sp.length > 0 ? sp[sp.length - 1] : null;
    });
</script>

<div class="deviation-panel">
    <div class="uc-panel-header">
        <span class="uc-panel-title">Deviation Map</span>
        <span class="uc-panel-sub">
            {#if refLat !== null}
                ref: {refLat.toFixed(6)}, {refLon!.toFixed(6)}
            {:else}
                acquiring reference...
            {/if}
        </span>
    </div>
    <div class="deviation-body">
        <svg viewBox="0 0 {SIZE} {SIZE}" class="deviation-svg">
            <!-- Background -->
            <rect x="0" y="0" width={SIZE} height={SIZE} fill="#111" />

            <!-- Concentric rings -->
            {#each ringSteps() as ringR}
                {@const r = ringR * scale()}
                <circle cx={CX} cy={CY} r={r} fill="none" stroke="#333" stroke-width="0.5" stroke-dasharray="2,2" />
                <text x={CX + 4} y={CY - r + 8} fill="#555" font-size="7" font-family="monospace">{ringR}m</text>
            {/each}

            <!-- Center crosshair -->
            <line x1={CX - 5} y1={CY} x2={CX + 5} y2={CY} stroke="#444" stroke-width="0.5" />
            <line x1={CX} y1={CY - 5} x2={CX} y2={CY + 5} stroke="#444" stroke-width="0.5" />

            <!-- Cardinal lines -->
            <line x1={CX} y1={CY - MAX_R} x2={CX} y2={CY + MAX_R} stroke="#222" stroke-width="0.5" />
            <line x1={CX - MAX_R} y1={CY} x2={CX + MAX_R} y2={CY} stroke="#222" stroke-width="0.5" />

            <!-- Cardinal labels -->
            <text x={CX} y={CY - MAX_R - 4} fill="#888" font-size="9" font-weight="bold" text-anchor="middle">N</text>
            <text x={CX + MAX_R + 10} y={CY + 3} fill="#888" font-size="9" font-weight="bold" text-anchor="middle">E</text>
            <text x={CX} y={CY + MAX_R + 14} fill="#888" font-size="9" font-weight="bold" text-anchor="middle">S</text>
            <text x={CX - MAX_R - 10} y={CY + 3} fill="#888" font-size="9" font-weight="bold" text-anchor="middle">W</text>

            <!-- Path -->
            {#if pathD()}
                <path d={pathD()} fill="none" stroke="#00c853" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" opacity="0.9" />
            {/if}

            <!-- Last position dot -->
            {#if lastPoint()}
                <circle cx={lastPoint()!.x} cy={lastPoint()!.y} r="3" fill="#00c853" stroke="#fff" stroke-width="1" />
            {/if}
        </svg>

        <div class="deviation-stats">
            <div class="dev-stat">
                <span class="dev-stat-label">Samples</span>
                <span class="dev-stat-value">{positionHistory.length}</span>
            </div>
            <div class="dev-stat">
                <span class="dev-stat-label">Max Dev</span>
                <span class="dev-stat-value">{maxDist().toFixed(2)} m</span>
            </div>
            <div class="dev-stat">
                <span class="dev-stat-label">Scale</span>
                <span class="dev-stat-value">±{maxRing().toFixed(1)} m</span>
            </div>
        </div>
    </div>
</div>

<style lang="scss">
    .deviation-panel {
        background: white;
        border-bottom: 1px solid #ddd;
    }
    .deviation-body {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        padding: 8px;
        align-items: flex-start;
    }
    .deviation-svg {
        width: 260px;
        height: 260px;
        flex-shrink: 0;
    }
    .deviation-stats {
        display: flex;
        flex-direction: column;
        gap: 6px;
        padding: 4px;
    }
    .dev-stat {
        display: flex;
        justify-content: space-between;
        gap: 12px;
        font-size: 0.8em;
        padding: 4px 8px;
        background: #f8f8f8;
        border-radius: 3px;
        min-width: 120px;
    }
    .dev-stat-label {
        color: #888;
    }
    .dev-stat-value {
        color: #333;
        font-family: monospace;
        font-weight: 600;
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
