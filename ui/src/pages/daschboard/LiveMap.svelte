<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import { page } from '$app/stores';
    import { afterNavigate } from '$app/navigation';
    import { browser } from '$app/environment';
    import { socketService } from '../../stores/socket';
    import { gpsStore } from '../../stores/gpsStore';
    import type { PositionSample } from '../../stores/gpsStore';

    const TIME_WINDOWS: { label: string; ms: number }[] = [
        { label: '5 min', ms: 5 * 60 * 1000 },
        { label: '15 min', ms: 15 * 60 * 1000 },
        { label: '30 min', ms: 30 * 60 * 1000 },
        { label: '1 h', ms: 60 * 60 * 1000 },
        { label: '2 h', ms: 2 * 60 * 60 * 1000 },
        { label: 'All', ms: Infinity },
    ];

    let selectedWindow = $state(TIME_WINDOWS[3].ms);
    let showAccuracy = $state(true);

    let mapContainer: HTMLDivElement;
    let L: any = null;
    let map: any = null;
    let trackPolyline: any = null;
    let accuracyCircle: any = null;
    let roverMarker: any = null;
    let gpsState = $derived($gpsStore);

    let filteredHistory = $derived(() => {
        const now = Date.now();
        if (selectedWindow === Infinity) return gpsState.positionHistory;
        const cutoff = now - selectedWindow;
        return gpsState.positionHistory.filter(p => p.time >= cutoff);
    });

    function initMap() {
        if (!browser || !mapContainer) return;

        import('leaflet').then((leafletModule) => {
            L = leafletModule.default || leafletModule;

            map = L.map(mapContainer, {
                zoomControl: false,
                attributionControl: true,
            }).setView([51.1657, 10.4515], 6);

            L.control.zoom({ position: 'bottomright' }).addTo(map);

            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                maxZoom: 19,
                attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
            }).addTo(map);

            updateLayers();
        });
    }

    function updateLayers() {
        if (!map || !L) return;

        const history = filteredHistory();
        const pvt = gpsState.navPvt;

        // ─── Track Polyline ─────────────────────────────────────────────
        const trackLatLngs = history.map((p: PositionSample) => [p.lat, p.lon]);

        if (trackPolyline) {
            map.removeLayer(trackPolyline);
            trackPolyline = null;
        }

        if (trackLatLngs.length >= 2) {
            trackPolyline = L.polyline(trackLatLngs, {
                color: '#006064',
                weight: 3,
                opacity: 0.85,
                lineCap: 'round',
                lineJoin: 'round',
            }).addTo(map);
        }

        // ─── Rover Marker + Accuracy ────────────────────────────────────
        if (roverMarker) { map.removeLayer(roverMarker); roverMarker = null; }
        if (accuracyCircle) { map.removeLayer(accuracyCircle); accuracyCircle = null; }

        if (pvt && pvt.fixOk && pvt.fixType >= 2) {
            const pos: [number, number] = [pvt.lat, pvt.lon];

            const roverIcon = L.divIcon({
                className: 'rover-marker',
                html: `<div class="rover-marker-inner" style="transform: rotate(${pvt.heading}deg)">
                        <div class="rover-arrow"></div>
                        <div class="rover-body"></div>
                       </div>`,
                iconSize: [24, 24],
                iconAnchor: [12, 12],
            });

            roverMarker = L.marker(pos, { icon: roverIcon, zIndexOffset: 1000 }).addTo(map);

            if (showAccuracy && pvt.hAcc > 0) {
                accuracyCircle = L.circle(pos, {
                    radius: pvt.hAcc,
                    color: '#006064',
                    fillColor: '#006064',
                    fillOpacity: 0.15,
                    weight: 1,
                    dashArray: '4,4',
                }).addTo(map);
            }
        }

        // ─── Auto-fit or center ─────────────────────────────────────────
        if (trackLatLngs.length >= 2) {
            const bounds = L.latLngBounds(trackLatLngs);
            if (pvt && pvt.fixOk) bounds.extend([pvt.lat, pvt.lon]);
            if (!map._livemap_fitted) {
                map.fitBounds(bounds, { padding: [40, 40], maxZoom: 18 });
                map._livemap_fitted = true;
            }
        } else if (pvt && pvt.fixOk) {
            map.setView([pvt.lat, pvt.lon], 18);
            map._livemap_fitted = true;
        }
    }

    // React to store changes
    $effect(() => {
        if (map && L) {
            updateLayers();
        }
    });

    // ─── Lifecycle / visibility ───────────────────────────────────────────
    let lastLivemapActive = false;
    function syncLivemapPolling() {
        if (!browser) return;
        const dashboard = $page.url.searchParams.get('dashboard');
        const isLivemap = dashboard === 'livemap';
        if (isLivemap && !lastLivemapActive) {
            socketService.requestGpsDetails();
            gpsStore.connect();
            if (!map) initMap();
            lastLivemapActive = true;
        } else if (!isLivemap && lastLivemapActive) {
            socketService.stopGpsDetails();
            gpsStore.disconnect();
            lastLivemapActive = false;
        }
    }
    onMount(() => {
        syncLivemapPolling();
    });
    afterNavigate(syncLivemapPolling);

    onDestroy(() => {
        if (lastLivemapActive) {
            socketService.stopGpsDetails();
            gpsStore.disconnect();
            lastLivemapActive = false;
        }
        if (map) {
            map.remove();
            map = null;
        }
    });

    function onWindowChange() {
        if (map && L) {
            map._livemap_fitted = false;
            updateLayers();
        }
    }
</script>

<div class="livemap-container">
    <!-- Toolbar -->
    <div class="livemap-toolbar">
        <div class="livemap-title">🗺️ Live Map</div>
        <div class="livemap-controls">
            <label class="livemap-label">
                Track
                <select bind:value={selectedWindow} onchange={onWindowChange}>
                    {#each TIME_WINDOWS as w}
                        <option value={w.ms}>{w.label}</option>
                    {/each}
                </select>
            </label>
            <label class="livemap-check">
                <input type="checkbox" bind:checked={showAccuracy} onchange={updateLayers} />
                Accuracy
            </label>

            <div class="livemap-stats">
                {#if gpsState.navPvt}
                    {@const p = gpsState.navPvt}
                    <span class="stat-item">{(p.lat ?? 0).toFixed(6)}°, {(p.lon ?? 0).toFixed(6)}°</span>
                    <span class="stat-item">±{(p.hAcc ?? 0).toFixed(2)} m</span>
                    <span class="stat-item">{(p.gSpeed ?? 0).toFixed(2)} m/s</span>
                {:else}
                    <span class="stat-item stat-wait">Waiting for GPS…</span>
                {/if}
            </div>
        </div>
    </div>

    <!-- Map container -->
    <div bind:this={mapContainer} class="livemap-map"></div>
</div>

<svelte:head>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"
        integrity="sha256-p4NxAoJBhIIN+hmNHrzRCf9tD/miZyoHS5obTRR9BMY="
        crossorigin="" />
</svelte:head>

<style lang="scss">
    .livemap-container {
        display: flex;
        flex-direction: column;
        width: 100%;
        height: 100vh;
        padding-top: 48px;
        box-sizing: border-box;
    }

    .livemap-toolbar {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 12px;
        padding: 8px 12px;
        background: white;
        border-bottom: 1px solid #ddd;
        z-index: 10;
    }

    .livemap-title {
        font-weight: 600;
        font-size: 0.95em;
        color: #333;
    }

    .livemap-controls {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 12px;
        flex: 1;
    }

    .livemap-label {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 0.8em;
        color: #555;
    }

    .livemap-label select {
        padding: 4px 8px;
        border: 1px solid #ccc;
        border-radius: 4px;
        font-size: 0.9em;
    }

    .livemap-check {
        display: flex;
        align-items: center;
        gap: 4px;
        font-size: 0.8em;
        color: #555;
        cursor: pointer;
    }

    .livemap-stats {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        margin-left: auto;
    }

    .stat-item {
        font-size: 0.75em;
        padding: 3px 8px;
        background: #f4f4f4;
        border-radius: 3px;
        font-family: monospace;
        color: #333;
    }

    .stat-wait {
        color: #888;
        font-style: italic;
    }

    .livemap-map {
        flex: 1;
        width: 100%;
        min-height: 0;
    }

    :global(.rover-marker) {
        background: transparent !important;
        border: none !important;
    }

    :global(.rover-marker-inner) {
        width: 24px;
        height: 24px;
        position: relative;
    }

    :global(.rover-body) {
        width: 12px;
        height: 12px;
        background: #006064;
        border: 2px solid white;
        border-radius: 50%;
        position: absolute;
        top: 6px;
        left: 6px;
        box-shadow: 0 1px 3px rgba(0,0,0,0.3);
    }

    :global(.rover-arrow) {
        width: 0;
        height: 0;
        border-left: 5px solid transparent;
        border-right: 5px solid transparent;
        border-bottom: 10px solid #006064;
        position: absolute;
        top: -2px;
        left: 7px;
    }

    :global(.leaflet-container) {
        font-family: inherit;
    }
</style>
