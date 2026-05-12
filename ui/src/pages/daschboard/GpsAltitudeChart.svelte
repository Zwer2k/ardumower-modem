<script lang="ts">
    import type { AltitudeSample } from '../../stores/gpsStore';

    interface Props {
        history: AltitudeSample[];
    }

    let { history }: Props = $props();

    const WIDTH = 400;
    const HEIGHT = 160;
    const PAD_L = 40;
    const PAD_R = 10;
    const PAD_T = 10;
    const PAD_B = 24;
    const GRAPH_W = WIDTH - PAD_L - PAD_R;
    const GRAPH_H = HEIGHT - PAD_T - PAD_B;

    let data = $derived(() => {
        if (history.length < 2) return [];
        return history;
    });

    let minH = $derived(() => {
        const d = data();
        if (d.length === 0) return 0;
        let min = Infinity;
        for (const p of d) {
            if (p.height < min) min = p.height;
            if (p.hMSL < min) min = p.hMSL;
        }
        return min;
    });

    let maxH = $derived(() => {
        const d = data();
        if (d.length === 0) return 10;
        let max = -Infinity;
        for (const p of d) {
            if (p.height > max) max = p.height;
            if (p.hMSL > max) max = p.hMSL;
        }
        return max;
    });

    let range = $derived(() => {
        const r = maxH() - minH();
        return r < 1 ? 1 : r;
    });

    let yTicks = $derived(() => {
        const mn = minH();
        const mx = maxH();
        const r = range();
        const step = r <= 2 ? 0.5 : r <= 5 ? 1 : r <= 10 ? 2 : r <= 20 ? 5 : 10;
        const start = Math.floor(mn / step) * step;
        const ticks: number[] = [];
        for (let v = start; v <= mx + step / 2; v += step) {
            ticks.push(parseFloat(v.toFixed(2)));
        }
        return ticks;
    });

    function mapX(i: number, total: number) {
        if (total <= 1) return PAD_L + GRAPH_W / 2;
        return PAD_L + (i / (total - 1)) * GRAPH_W;
    }

    function mapY(val: number) {
        const mn = minH();
        const r = range();
        return PAD_T + GRAPH_H - ((val - mn) / r) * GRAPH_H;
    }

    let pathEllip = $derived(() => {
        const d = data();
        if (d.length < 2) return "";
        return d.map((p, i) => `${i === 0 ? "M" : "L"}${mapX(i, d.length)},${mapY(p.height)}`).join(" ");
    });

    let pathMSL = $derived(() => {
        const d = data();
        if (d.length < 2) return "";
        return d.map((p, i) => `${i === 0 ? "M" : "L"}${mapX(i, d.length)},${mapY(p.hMSL)}`).join(" ");
    });
</script>

<div class="altitude-panel">
    <div class="uc-panel-header">
        <span class="uc-panel-title">Altitude</span>
        <span class="uc-panel-sub">
            {#if history.length > 0}
                Ell: {history[history.length - 1].height.toFixed(2)} m | MSL: {history[history.length - 1].hMSL.toFixed(2)} m
            {:else}
                no data
            {/if}
        </span>
    </div>
    <div class="altitude-body">
        <svg viewBox="0 0 {WIDTH} {HEIGHT}" class="altitude-svg">
            <!-- Grid -->
            {#each yTicks() as tick}
                {@const y = mapY(tick)}
                <line x1={PAD_L} y1={y} x2={WIDTH - PAD_R} y2={y} stroke="#eee" stroke-width="0.5" />
                <text x={PAD_L - 4} y={y + 3} fill="#888" font-size="8" font-family="monospace" text-anchor="end">{tick.toFixed(1)}</text>
            {/each}

            <!-- Axes -->
            <line x1={PAD_L} y1={PAD_T} x2={PAD_L} y2={HEIGHT - PAD_B} stroke="#ccc" stroke-width="0.5" />
            <line x1={PAD_L} y1={HEIGHT - PAD_B} x2={WIDTH - PAD_R} y2={HEIGHT - PAD_B} stroke="#ccc" stroke-width="0.5" />

            <!-- Ellipsoid height line -->
            {#if pathEllip()}
                <path d={pathEllip()} fill="none" stroke="#1565c0" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" />
            {/if}

            <!-- MSL height line -->
            {#if pathMSL()}
                <path d={pathMSL()} fill="none" stroke="#2e7d32" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" stroke-dasharray="3,2" />
            {/if}
        </svg>

        <div class="alt-legend">
            <div class="alt-legend-item">
                <span class="alt-legend-line" style="background: #1565c0"></span>
                <span>Ellipsoid</span>
            </div>
            <div class="alt-legend-item">
                <span class="alt-legend-line" style="background: #2e7d32; border-style: dashed"></span>
                <span>MSL</span>
            </div>
        </div>
    </div>
</div>

<style lang="scss">
    .altitude-panel {
        background: white;
        border-bottom: 1px solid #ddd;
    }
    .altitude-body {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        padding: 8px;
        align-items: center;
    }
    .altitude-svg {
        width: 100%;
        max-width: 420px;
        height: 160px;
        flex-shrink: 0;
    }
    .alt-legend {
        display: flex;
        flex-direction: column;
        gap: 6px;
        padding: 4px;
        font-size: 0.8em;
    }
    .alt-legend-item {
        display: flex;
        align-items: center;
        gap: 6px;
        color: #555;
    }
    .alt-legend-line {
        width: 20px;
        height: 2px;
        border-radius: 1px;
        display: inline-block;
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
