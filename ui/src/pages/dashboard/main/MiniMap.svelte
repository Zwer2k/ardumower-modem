<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import { select } from 'd3-selection';
    import { zoom, zoomIdentity, type ZoomTransform } from 'd3-zoom';
    import { MapStore, drivenTrackStore } from '../../../map/service';
    import { socketStore } from '../../../stores/socket';

    export let mapCrc: number | null | undefined = null;

    $: map = $MapStore?.map;
    $: presentation = $MapStore?.presentation;
    $: mowerPos = $socketStore?.state?.position ?? null;
    $: track = $drivenTrackStore;
    $: mapRotation = $socketStore?.currentMapMeta?.rotation ?? 0;

    $: mx = mowerPos?.x ?? 0;
    $: my = mowerPos != null ? -mowerPos.y : 0;
    $: headingDeg = mowerPos ? 90 - mowerPos.delta * 180 / Math.PI : 0;

    let svgEl: SVGSVGElement | null = null;
    let currentTransform: ZoomTransform = zoomIdentity;
    let cleanup: (() => void) | null = null;
    let compassOffset = 0;

    $: viewBoxValid = isViewBoxValid(presentation?.viewBox);

    $: transformAttr = viewBoxValid
        ? `translate(${currentTransform.x.toFixed(4)}, ${currentTransform.y.toFixed(4)}) scale(${currentTransform.k.toFixed(4)})`
        : '';

    $: vb = viewBoxValid && presentation?.viewBox ? presentation.viewBox.split(/\s+/).map(Number) : [0, 0, 1, 1];
    $: cx = vb[0] + vb[2] / 2;
    $: cy = vb[1] + vb[3] / 2;
    $: totalRotation = mapRotation + compassOffset;
    $: rotationAttr = viewBoxValid && totalRotation
        ? `rotate(${totalRotation}, ${cx}, ${cy})`
        : '';

    function isViewBoxValid(viewBox: string | undefined): boolean {
        if (!viewBox) return false;
        const parts = viewBox.split(/\s+/).map(Number);
        return parts.length === 4 && parts.every((n) => Number.isFinite(n));
    }

    function attachZoom(node: SVGSVGElement) {
        const z = zoom<SVGSVGElement, unknown>()
            .scaleExtent([0.5, 10])
            .on('zoom', ({ transform }) => {
                currentTransform = transform;
            });
        const sel = select(node);
        sel.call(z);
        return () => sel.on('.zoom', null);
    }

    $: if (svgEl && viewBoxValid) {
        cleanup = attachZoom(svgEl);
    } else if (cleanup) {
        cleanup();
        cleanup = null;
        currentTransform = zoomIdentity;
    }

    onDestroy(() => {
        cleanup?.();
    });

    function resetZoom() {
        if (!svgEl) return;
        const z = zoom<SVGSVGElement, unknown>()
            .scaleExtent([0.5, 10])
            .on('zoom', ({ transform }) => {
                currentTransform = transform;
            });
        select(svgEl).transition().duration(250).call(z.transform, zoomIdentity);
    }

    function resetCompass() {
        compassOffset = 0;
    }

    function onCompassDown(event: MouseEvent) {
        event.preventDefault();
        const btn = event.currentTarget as HTMLElement;
        const rect = btn.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;
        const startAngle = Math.atan2(event.clientY - centerY, event.clientX - centerX) * 180 / Math.PI;
        let lastAngle = startAngle;
        let didDrag = false;

        function onMove(e: MouseEvent) {
            didDrag = true;
            const angle = Math.atan2(e.clientY - centerY, e.clientX - centerX) * 180 / Math.PI;
            let delta = angle - lastAngle;
            if (delta > 180) delta -= 360;
            if (delta < -180) delta += 360;
            compassOffset = ((compassOffset + delta) % 360 + 360) % 360;
            lastAngle = angle;
        }

        function onUp() {
            window.removeEventListener('mousemove', onMove);
            window.removeEventListener('mouseup', onUp);
            if (!didDrag) {
                compassOffset = ((compassOffset + 90) % 360 + 360) % 360;
            }
        }

        window.addEventListener('mousemove', onMove);
        window.addEventListener('mouseup', onUp);
    }

    function arrowPoints(x: number, y: number, deg: number): string {
        const r = 0.25;
        const rad = (deg * Math.PI) / 180;
        const tipX = x + r * Math.cos(rad);
        const tipY = y - r * Math.sin(rad);
        const wing1 = ((deg + 150) * Math.PI) / 180;
        const wing2 = ((deg - 150) * Math.PI) / 180;
        const w1x = x + r * 0.5 * Math.cos(wing1);
        const w1y = y - r * 0.5 * Math.sin(wing1);
        const w2x = x + r * 0.5 * Math.cos(wing2);
        const w2y = y - r * 0.5 * Math.sin(wing2);
        return `${tipX},${tipY} ${w1x},${w1y} ${w2x},${w2y}`;
    }
</script>

<div class="mini-map">
    {#if map?.perimeter?.points?.length > 0 && viewBoxValid}
        <div class="map-status" class:synced={mapCrc != null}>
            {mapCrc != null ? 'Karte OK' : 'Keine Karte'}
        </div>
        <button class="compass-btn" on:mousedown={onCompassDown} on:dblclick={resetCompass} title="Karte drehen (ziehen für fein, Doppelklick zurücksetzen)">
            <svg viewBox="-12 -12 24 24" width="20" height="20">
                <g transform="rotate({totalRotation})">
                    <circle cx="0" cy="0" r="10" fill="white" stroke="#999" stroke-width="1.5"/>
                    <polygon points="0,-8 -4,0 0,-2 4,0" fill="#d32f2f"/>
                    <polygon points="0,8 -4,0 0,2 4,0" fill="#999"/>
                </g>
            </svg>
        </button>
        <button class="zoom-reset" on:click={resetZoom} title="Zoom zurücksetzen">⟲</button>
        <svg bind:this={svgEl} viewBox={presentation.viewBox} preserveAspectRatio="xMidYMid meet">
            <g transform={transformAttr}>
                <g transform={rotationAttr}>
                    <polygon
                        points={map.perimeter.points.map((p) => `${p.x},${p.y}`).join(' ')}
                        fill="rgba(36, 161, 72, 0.15)"
                        stroke="#24a148"
                        stroke-width="0.04"
                    />

                {#each map.exclusions ?? [] as ex}
                    <polygon
                        points={ex.points.map((p) => `${p.x},${p.y}`).join(' ')}
                        fill="rgba(218, 30, 40, 0.15)"
                        stroke="#da1e28"
                        stroke-width="0.03"
                    />
                {/each}

                {#if map.dockpoints?.points?.length}
                    <polyline
                        points={map.dockpoints.points.map((p) => `${p.x},${p.y}`).join(' ')}
                        fill="none"
                        stroke="#ff832b"
                        stroke-width="0.02"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                    />
                {/if}

                {#if map.waypoints?.points?.length}
                    <polyline
                        points={map.waypoints.points.map((p) => `${p.x},${p.y}`).join(' ')}
                        fill="none"
                        stroke="#6929c4"
                        stroke-width="0.015"
                        stroke-dasharray="0.15 0.10"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                    />
                {/if}

                </g>
                <g transform={rotationAttr}>
                    {#if track?.points?.length}
                        <polyline
                            points={track.points.map((p) => `${p.x},${-p.y}`).join(' ')}
                            fill="none"
                            stroke="rgba(0, 200, 255, 0.5)"
                            stroke-width="0.05"
                        />
                    {/if}

                    {#if mowerPos}
                        <g transform="translate({mx}, {my})">
                            <circle r="0.22" fill="none" stroke="#00c8ff" stroke-width="0.05" />
                            <circle r="0.14" fill="#00c8ff" />
                            <polygon points={arrowPoints(0, 0, headingDeg)} fill="white" />
                        </g>
                    {/if}
                </g>
            </g>
        </svg>
    {:else}
        <div class="no-map">No map loaded</div>
    {/if}
</div>

<style lang="scss">
    .mini-map {
        width: 100%;
        height: 100%;
        min-height: 200px;
        display: flex;
        align-items: center;
        justify-content: center;
        background: #f4f4f4;
        border-radius: 0;
        overflow: hidden;
        position: relative;
    }

    .map-status {
        position: absolute;
        top: 8px;
        right: 8px;
        padding: 2px 8px;
        border-radius: 12px;
        font-size: 0.625rem;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.03em;
        background: #da1e28;
        color: white;
        z-index: 2;
    }

    .map-status.synced {
        background: #24a148;
    }

    .zoom-reset,
    .compass-btn {
        position: absolute;
        width: 28px;
        height: 28px;
        border-radius: 50%;
        border: none;
        background: white;
        box-shadow: 0 1px 4px rgba(0,0,0,0.2);
        display: flex;
        align-items: center;
        justify-content: center;
        cursor: pointer;
        z-index: 2;
    }

    .zoom-reset {
        bottom: 8px;
        right: 8px;
        font-size: 1rem;
    }

    .compass-btn {
        top: 8px;
        left: 8px;
        padding: 0;
    }

    svg {
        width: 100%;
        height: 100%;
        display: block;
        cursor: grab;
    }

    svg:active {
        cursor: grabbing;
    }

    .no-map {
        color: #525252;
        font-size: 0.875rem;
    }
</style>
