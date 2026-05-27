<script lang="ts">
    import type { NavSatInfo } from '../../../stores/gpsStore';

    interface Props {
        satellites: NavSatInfo[];
    }

    let { satellites }: Props = $props();

    const SIZE = 300;
    const CX = SIZE / 2;
    const CY = SIZE / 2;
    const MAX_R = SIZE / 2 - 20;

    function gnssColor(gnssId: number): string {
        switch (gnssId) {
            case 0: return '#1565c0'; // GPS
            case 1: return '#e65100'; // SBAS
            case 2: return '#2e7d32'; // Galileo
            case 3: return '#c2185b'; // BeiDou
            case 4: return '#7b1fa2'; // IMES
            case 5: return '#00695c'; // QZSS
            case 6: return '#283593'; // GLONASS
            default: return '#555';
        }
    }

    function gnssName(gnssId: number): string {
        switch (gnssId) {
            case 0: return 'GPS';
            case 1: return 'SBAS';
            case 2: return 'Gal';
            case 3: return 'BDS';
            case 4: return 'IMES';
            case 5: return 'QZSS';
            case 6: return 'GLO';
            default: return `G${gnssId}`;
        }
    }

    function satPos(sat: NavSatInfo) {
        // Elevation: 90° = center, 0° = outer edge
        const r = (1 - sat.elev / 90) * MAX_R;
        // Azimuth: 0° = North (up), clockwise
        const rad = (sat.azim * Math.PI) / 180;
        const x = CX + r * Math.sin(rad);
        const y = CY - r * Math.cos(rad);
        return { x, y, r };
    }

    const elevationRings = [0, 15, 30, 45, 60, 75, 90];
    const azimuthLines = [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330];
    const cardinalLabels = [
        { label: 'N', x: CX, y: CY - MAX_R - 8 },
        { label: 'E', x: CX + MAX_R + 12, y: CY + 4 },
        { label: 'S', x: CX, y: CY + MAX_R + 14 },
        { label: 'W', x: CX - MAX_R - 12, y: CY + 4 },
    ];

    let groupedSats = $derived.by(() => {
        const groups: Record<number, NavSatInfo[]> = {};
        for (const s of satellites) {
            if (!groups[s.gnssId]) groups[s.gnssId] = [];
            groups[s.gnssId].push(s);
        }
        return groups;
    });
</script>

<div class="skyplot-panel">
    <div class="uc-panel-header">
        <span class="uc-panel-title">Skyplot</span>
        <span class="uc-panel-sub">{satellites.length} satellites visible</span>
    </div>
    <div class="skyplot-body">
        <svg viewBox="0 0 {SIZE} {SIZE}" class="skyplot-svg">
            <!-- Background -->
            <circle cx={CX} cy={CY} r={MAX_R} fill="#111" stroke="#333" stroke-width="1" />

            <!-- Elevation rings -->
            {#each elevationRings as elev}
                {@const ringR = (1 - elev / 90) * MAX_R}
                <circle cx={CX} cy={CY} r={ringR} fill="none" stroke="#333" stroke-width="0.5" stroke-dasharray={elev === 0 ? 'none' : '2,2'} />
                {#if elev > 0 && elev < 90}
                    <text x={CX + 2} y={CY - ringR + 10} fill="#666" font-size="8" font-family="monospace">{elev}°</text>
                {/if}
            {/each}

            <!-- Azimuth lines -->
            {#each azimuthLines as az}
                {@const rad = (az * Math.PI) / 180}
                {@const x2 = CX + MAX_R * Math.sin(rad)}
                {@const y2 = CY - MAX_R * Math.cos(rad)}
                <line x1={CX} y1={CY} x2={x2} y2={y2} stroke="#333" stroke-width="0.5" />
            {/each}

            <!-- Cardinal labels -->
            {#each cardinalLabels as c}
                <text x={c.x} y={c.y} fill="#888" font-size="10" font-weight="bold" text-anchor="middle">{c.label}</text>
            {/each}

            <!-- Azimuth degree labels -->
            {#each [0, 45, 90, 135, 180, 225, 270, 315] as az}
                {@const rad = (az * Math.PI) / 180}
                {@const lx = CX + (MAX_R + 4) * Math.sin(rad)}
                {@const ly = CY - (MAX_R + 4) * Math.cos(rad)}
                <text x={lx} y={ly + 3} fill="#555" font-size="7" font-family="monospace" text-anchor="middle">{az}°</text>
            {/each}

            <!-- Satellites -->
            {#each satellites as sat}
                {@const pos = satPos(sat)}
                {@const color = gnssColor(sat.gnssId)}
                {@const size = Math.max(6, Math.min(14, sat.cno / 4))}
                {@const opacity = sat.elev > 0 ? 1 : 0.3}
                <g transform="translate({pos.x}, {pos.y})" opacity={opacity}>
                    <!-- Satellite block -->
                    <rect x={-size/2} y={-size/2} width={size} height={size} rx="1" fill={color} stroke={sat.used ? '#fff' : 'none'} stroke-width={sat.used ? 1.5 : 0} />
                    <!-- SV ID -->
                    <text y={size/2 + 10} fill={color} font-size="8" font-family="monospace" text-anchor="middle" font-weight="bold">{sat.svId}</text>
                </g>
            {/each}
        </svg>

        <!-- Legend -->
        <div class="skyplot-legend">
            {#each Object.entries(groupedSats) as [gnssId, sats]}
                {@const id = parseInt(gnssId)}
                {@const color = gnssColor(id)}
                <div class="legend-item">
                    <span class="legend-color" style="background: {color}"></span>
                    <span class="legend-name">{gnssName(id)}</span>
                    <span class="legend-count">{sats.length}</span>
                </div>
            {/each}
        </div>
    </div>
</div>

<style lang="scss">
    .skyplot-panel {
        background: white;
        border-bottom: 1px solid #ddd;
    }
    .skyplot-body {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        padding: 8px;
        align-items: flex-start;
    }
    .skyplot-svg {
        width: 280px;
        height: 280px;
        flex-shrink: 0;
    }
    .skyplot-legend {
        display: grid;
        grid-template-columns: repeat(3, auto);
        gap: 4px 16px;
        padding: 4px;
    }
    .legend-item {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 0.8em;
    }
    .legend-color {
        width: 12px;
        height: 12px;
        border-radius: 2px;
        display: inline-block;
    }
    .legend-name {
        color: #333;
        min-width: 40px;
    }
    .legend-count {
        color: #888;
        font-family: monospace;
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
