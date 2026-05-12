<script lang="ts">
    import { onMount, onDestroy } from 'svelte';
    import { socketStore, socketService } from '../../stores/socket';
    import {
        ubxCommands,
        parseUbxFrames,
        findUbxFrames,
        hexDump,
        getParserForFrame,
        type UbxFrameResult,
    } from './ubxCommands';
    import type { GpsDetails } from '../../model';

    // ─── Types ──────────────────────────────────────────────────────────────

    type ConfigCategory = 'receiver' | 'navigation' | 'satellites' | 'ports' | 'gnss' | 'rate';

    interface ConfigCard {
        id: string;
        title: string;
        category: ConfigCategory;
        commandId: string;
        data: Record<string, string> | Array<Record<string, string | number>> | null;
        timestamp: number;
        loading: boolean;
        error: string;
    }

    // ─── State ──────────────────────────────────────────────────────────────

    let cards: ConfigCard[] = $state([
        { id: 'mon-ver', title: 'Receiver Version', category: 'receiver', commandId: 'mon-ver', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'mon-hw', title: 'Hardware Status', category: 'receiver', commandId: 'mon-hw', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'mon-rf', title: 'RF Status', category: 'receiver', commandId: 'mon-rf', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'mon-comms', title: 'Port Traffic', category: 'receiver', commandId: 'mon-comms', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'nav-pvt', title: 'PVT Solution', category: 'navigation', commandId: 'nav-pvt', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'nav-dop', title: 'DOP Values', category: 'navigation', commandId: 'nav-dop', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'nav-status', title: 'Fix Status', category: 'navigation', commandId: 'nav-status', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-nav5-get', title: 'Nav Engine', category: 'navigation', commandId: 'cfg-nav5-get', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-rate-get', title: 'Measurement Rate', category: 'rate', commandId: 'cfg-rate-get', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-rate', title: 'Rate Config', category: 'rate', commandId: 'cfg-valget-rate', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'nav-sat', title: 'Satellite Details', category: 'satellites', commandId: 'nav-sat', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-port1', title: 'UART1 Baudrate', category: 'ports', commandId: 'cfg-valget-port1', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-uart1-proto', title: 'UART1 Protocols', category: 'ports', commandId: 'cfg-valget-uart1-proto', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-gnss', title: 'GNSS Systems', category: 'gnss', commandId: 'cfg-valget-gnss', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-sbas', title: 'SBAS', category: 'gnss', commandId: 'cfg-valget-sbas', data: null, timestamp: 0, loading: false, error: '' },
        { id: 'cfg-valget-rtcm', title: 'RTCM Input', category: 'gnss', commandId: 'cfg-valget-rtcm', data: null, timestamp: 0, loading: false, error: '' },
    ]);

    let activeCategory = $state<ConfigCategory | 'all'>('all');
    let expertMode = $state(false);
    let customHex = $state('');
    let expertResult: { frames: UbxFrameResult[]; rawHex: string } | null = $state(null);
    let showRawHex = $state(false);

    // Poll state
    let pollQueue: string[] = [];
    let pollTimer: ReturnType<typeof setTimeout> | null = null;
    let processedHex = '';

    // Map commandId -> card
    function cardByCmdId(cmdId: string): ConfigCard | undefined {
        return cards.find(c => c.commandId === cmdId);
    }

    // ─── Polling ────────────────────────────────────────────────────────────

    function refreshAll() {
        // Stop any ongoing poll
        if (pollTimer) {
            clearTimeout(pollTimer);
            pollTimer = null;
        }

        // Reset all cards
        for (const c of cards) {
            c.loading = true;
            c.error = '';
        }
        cards = [...cards];

        // Build queue (order matters: start with receiver info)
        pollQueue = cards.map(c => c.commandId);
        processedHex = '';
        sendNextPoll();
    }

    function sendNextPoll() {
        if (pollQueue.length === 0) {
            // Queue done: clear remaining loading states after grace period
            pollTimer = setTimeout(() => {
                for (const c of cards) {
                    if (c.loading) {
                        c.loading = false;
                        c.error = 'No response';
                    }
                }
                cards = [...cards];
            }, 3000);
            return;
        }

        const cmdId = pollQueue.shift()!;
        const cmd = ubxCommands.find(c => c.id === cmdId);
        if (!cmd || !cmd.hex) {
            const card = cardByCmdId(cmdId);
            if (card) { card.loading = false; card.error = 'No hex'; }
            cards = [...cards];
            sendNextPoll();
            return;
        }

        socketService.sendUbx(cmd.hex);

        // Wait 300ms before next poll to avoid flooding the GPS receiver
        pollTimer = setTimeout(() => {
            sendNextPoll();
        }, 300);
    }

    // ─── Socket handler ─────────────────────────────────────────────────────

    function handleUbxResponse(resp: { timestamp: number; hex: string }) {
        if (!resp || !resp.hex) return;
        const cleanHex = (resp.hex || '').replace(/[^0-9a-fA-F]/g, '');
        if (cleanHex.length === 0 || cleanHex === processedHex) return;
        processedHex = cleanHex;

        const frames = parseUbxFrames(cleanHex);

        for (const frame of frames) {
            if (!frame.valid) continue;

            // Find matching card(s) by parser
            const parserName = frame.msgName;
            let matched = false;

            for (const card of cards) {
                const cmd = ubxCommands.find(c => c.id === card.commandId);
                if (!cmd) continue;

                // Match by expected response frame
                const expectedParser = getParserForFrameByCommand(card.commandId);
                const actualParser = getParserForFrame(frame.classId, frame.msgId);
                if (expectedParser === actualParser && actualParser) {
                    try {
                        const data = actualParser(cleanHex);
                        card.data = data;
                        card.timestamp = Date.now();
                        card.loading = false;
                        card.error = '';
                        matched = true;
                    } catch (e) {
                        card.error = String(e);
                        card.loading = false;
                    }
                }
            }

            // Also populate expert mode result
            if (expertMode) {
                expertResult = {
                    frames,
                    rawHex: cleanHex,
                };
            }
        }

        cards = [...cards];
    }

    function getParserForFrameByCommand(commandId: string) {
        const cmd = ubxCommands.find(c => c.id === commandId);
        if (!cmd) return null;
        // Infer frame type from command id
        switch (commandId) {
            case 'mon-ver': return getParserForFrame(0x0a, 0x04);
            case 'mon-hw': return getParserForFrame(0x0a, 0x09);
            case 'mon-rf': return getParserForFrame(0x0a, 0x38);
            case 'mon-comms': return getParserForFrame(0x0a, 0x36);
            case 'nav-sat': return getParserForFrame(0x01, 0x35);
            case 'nav-sig': return getParserForFrame(0x01, 0x43);
            case 'nav-status': return getParserForFrame(0x01, 0x03);
            case 'nav-pvt': return getParserForFrame(0x01, 0x07);
            case 'nav-dop': return getParserForFrame(0x01, 0x04);
            case 'cfg-rate-get': return getParserForFrame(0x06, 0x08);
            case 'cfg-nav5-get': return getParserForFrame(0x06, 0x24);
            case 'cfg-valget-port1':
            case 'cfg-valget-sbas':
            case 'cfg-valget-rtcm':
            case 'cfg-valget-uart1-proto':
            case 'cfg-valget-gnss':
            case 'cfg-valget-rate':
                return getParserForFrame(0x06, 0x8b);
            default: return null;
        }
    }

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    let unsubscribeSocket: (() => void) | null = null;
    let processedUbxTimestamp = 0;

    onMount(() => {
        unsubscribeSocket = socketStore.subscribe((state) => {
            if (state.ubxResponse && state.ubxResponse.timestamp !== processedUbxTimestamp) {
                processedUbxTimestamp = state.ubxResponse.timestamp;
                handleUbxResponse(state.ubxResponse);
            }
        });

        // Auto-refresh on mount
        refreshAll();
    });

    onDestroy(() => {
        if (pollTimer) clearTimeout(pollTimer);
        if (unsubscribeSocket) {
            unsubscribeSocket();
            unsubscribeSocket = null;
        }
    });

    // ─── Derived ──────────────────────────────────────────────────────────────

    let filteredCards = $derived(
        activeCategory === 'all'
            ? cards
            : cards.filter(c => c.category === activeCategory)
    );

    let categoryCounts = $derived(() => {
        const counts: Record<string, number> = {};
        for (const c of cards) {
            counts[c.category] = (counts[c.category] || 0) + 1;
        }
        return counts;
    });

    let loadedCount = $derived(cards.filter(c => c.data !== null && !c.loading).length);
    let totalCount = $derived(cards.length);

    // ─── Helpers ──────────────────────────────────────────────────────────────

    function categoryLabel(cat: ConfigCategory): string {
        switch (cat) {
            case 'receiver': return 'Receiver';
            case 'navigation': return 'Navigation';
            case 'satellites': return 'Satellites';
            case 'ports': return 'Ports';
            case 'gnss': return 'GNSS';
            case 'rate': return 'Rate';
        }
    }

    function isTableData(data: any): data is Array<Record<string, string | number>> {
        return Array.isArray(data) && data.length > 0;
    }

    function isKvData(data: any): data is Record<string, string> {
        return data !== null && !Array.isArray(data) && typeof data === 'object';
    }

    function sendCustomUbx() {
        const hex = customHex.trim().replace(/\s/g, '');
        if (!hex || hex.length % 2 !== 0) return;
        expertResult = null;
        socketService.sendUbx(hex);
    }
</script>

<div class="adv-config">
    <!-- Toolbar -->
    <div class="adv-toolbar">
        <div class="adv-categories">
            <button class:active={activeCategory === 'all'} onclick={() => activeCategory = 'all'}>
                All ({totalCount})
            </button>
            {#each ['receiver', 'navigation', 'satellites', 'ports', 'gnss', 'rate'] as cat}
                <button class:active={activeCategory === cat} onclick={() => activeCategory = cat}>
                    {categoryLabel(cat as ConfigCategory)}
                </button>
            {/each}
        </div>
        <div class="adv-actions">
            <div class="adv-progress">{loadedCount}/{totalCount}</div>
            <button class="adv-refresh" onclick={refreshAll}>
                🔄 Refresh All
            </button>
            <button class="adv-expert-toggle" class:active={expertMode} onclick={() => expertMode = !expertMode}>
                {expertMode ? '✕ Expert' : '🔧 Expert'}
            </button>
        </div>
    </div>

    <!-- Config Cards Grid -->
    <div class="adv-grid">
        {#each filteredCards as card (card.id)}
            <div class="adv-card" class:loading={card.loading} class:error={!!card.error}>
                <div class="adv-card-header">
                    <span class="adv-card-title">{card.title}</span>
                    {#if card.loading}
                        <span class="adv-card-spinner">⏳</span>
                    {:else if card.error}
                        <span class="adv-card-badge error">!</span>
                    {:else if card.data}
                        <span class="adv-card-badge ok">✓</span>
                    {/if}
                </div>

                {#if card.error && !card.loading}
                    <div class="adv-card-error">{card.error}</div>
                {:else if isTableData(card.data)}
                    <div class="adv-card-table-wrap">
                        <table class="adv-card-table">
                            <thead>
                                <tr>
                                    {#each Object.keys(card.data[0]) as key}
                                        <th>{key}</th>
                                    {/each}
                                </tr>
                            </thead>
                            <tbody>
                                {#each card.data as row}
                                    <tr>
                                        {#each Object.values(row) as val}
                                            <td>{val}</td>
                                        {/each}
                                    </tr>
                                {/each}
                            </tbody>
                        </table>
                    </div>
                {:else if isKvData(card.data)}
                    <div class="adv-card-kv">
                        {#each Object.entries(card.data) as [key, val]}
                            <div class="adv-kv-row">
                                <span class="adv-kv-key">{key}</span>
                                <span class="adv-kv-val">{val}</span>
                            </div>
                        {/each}
                    </div>
                {:else}
                    <div class="adv-card-empty">
                        {#if card.loading}
                            Polling receiver...
                        {:else}
                            No data
                        {/if}
                    </div>
                {/if}
            </div>
        {/each}
    </div>

    <!-- Expert Mode Panel -->
    {#if expertMode}
        <div class="adv-expert-panel">
            <div class="adv-expert-header">
                <h4>UBX Expert Terminal</h4>
                <span class="adv-expert-hint">Send raw UBX hex commands directly to the GPS receiver</span>
            </div>
            <div class="adv-expert-input">
                <input type="text" placeholder="Hex bytes (e.g. B56206080000000E)" bind:value={customHex} />
                <button onclick={sendCustomUbx} disabled={!customHex}>Send</button>
            </div>

            {#if expertResult}
                <div class="adv-expert-frames">
                    {#each expertResult.frames as frame}
                        <div class="adv-expert-frame">
                            <div class="adv-expert-frame-header">
                                <span class="adv-badge {frame.valid ? 'valid' : 'invalid'}">{frame.valid ? '✓' : '✗'}</span>
                                <span class="adv-frame-name">{frame.msgName}</span>
                                <span class="adv-frame-meta">{frame.length} bytes</span>
                            </div>
                            {#if frame.specific}
                                {#if Array.isArray(frame.specific)}
                                    <table class="adv-mini-table">
                                        <thead>
                                            <tr>
                                                {#each Object.keys(frame.specific[0]) as k}
                                                    <th>{k}</th>
                                                {/each}
                                            </tr>
                                        </thead>
                                        <tbody>
                                            {#each frame.specific as row}
                                                <tr>
                                                    {#each Object.values(row) as v}
                                                        <td>{v}</td>
                                                    {/each}
                                                </tr>
                                            {/each}
                                        </tbody>
                                    </table>
                                {:else}
                                    <div class="adv-mini-kv">
                                        {#each Object.entries(frame.specific) as [k, v]}
                                            <div class="adv-kv-row"><span>{k}</span><span>{v}</span></div>
                                        {/each}
                                    </div>
                                {/if}
                            {/if}
                        </div>
                    {/each}
                </div>
                <button class="adv-raw-toggle" onclick={() => showRawHex = !showRawHex}>
                    {showRawHex ? '▾' : '▸'} Raw Hex ({expertResult.rawHex.length / 2} bytes)
                </button>
                {#if showRawHex}
                    <pre class="adv-raw-hex">{expertResult.rawHex}</pre>
                {/if}
            {/if}
        </div>
    {/if}
</div>

<style lang="scss">
    .adv-config {
        display: flex;
        flex-direction: column;
        gap: 10px;
        padding: 10px;
        background: #f0f0f0;
        min-height: 100%;
    }

    .adv-toolbar {
        display: flex;
        flex-wrap: wrap;
        gap: 10px;
        align-items: center;
        justify-content: space-between;
        background: white;
        padding: 10px 12px;
        border-radius: 6px;
        border: 1px solid #ddd;
    }

    .adv-categories {
        display: flex;
        flex-wrap: wrap;
        gap: 4px;
    }

    .adv-categories button {
        padding: 5px 12px;
        border: 1px solid #ccc;
        background: #f4f4f4;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.8em;
        color: #555;
        transition: all 0.15s ease;
    }

    .adv-categories button:hover {
        background: #e8e8e8;
    }

    .adv-categories button.active {
        background: #006064;
        color: white;
        border-color: #006064;
    }

    .adv-actions {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .adv-progress {
        font-size: 0.8em;
        color: #888;
        font-family: monospace;
        background: #f8f8f8;
        padding: 4px 8px;
        border-radius: 4px;
    }

    .adv-refresh {
        padding: 6px 14px;
        background: #006064;
        color: white;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85em;
    }

    .adv-refresh:hover {
        background: #004d4f;
    }

    .adv-expert-toggle {
        padding: 6px 12px;
        background: #f4f4f4;
        border: 1px solid #ccc;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85em;
        color: #555;
    }

    .adv-expert-toggle.active {
        background: #333;
        color: white;
        border-color: #333;
    }

    /* ─── Grid ─────────────────────────────────────────────────────────────── */

    .adv-grid {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
        gap: 10px;
    }

    .adv-card {
        background: white;
        border: 1px solid #ddd;
        border-radius: 6px;
        overflow: hidden;
        display: flex;
        flex-direction: column;
        min-height: 120px;
    }

    .adv-card.loading {
        border-left: 3px solid #ffab00;
    }

    .adv-card.error {
        border-left: 3px solid #c5221f;
    }

    .adv-card:not(.loading):not(.error) {
        border-left: 3px solid #00c853;
    }

    .adv-card-header {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 8px 10px;
        background: #f8f8f8;
        border-bottom: 1px solid #eee;
        font-size: 0.85em;
    }

    .adv-card-title {
        flex: 1;
        font-weight: 600;
        color: #333;
    }

    .adv-card-spinner {
        font-size: 0.9em;
        animation: spin 1s linear infinite;
    }

    @keyframes spin {
        from { transform: rotate(0deg); }
        to { transform: rotate(360deg); }
    }

    .adv-card-badge {
        font-size: 0.75em;
        width: 18px;
        height: 18px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 50%;
        font-weight: bold;
    }

    .adv-card-badge.ok {
        background: #e6f4ea;
        color: #1e8e3e;
    }

    .adv-card-badge.error {
        background: #fce8e8;
        color: #c5221f;
    }

    .adv-card-error {
        padding: 8px 10px;
        color: #c5221f;
        font-size: 0.8em;
        background: #fff5f5;
    }

    .adv-card-empty {
        padding: 16px;
        text-align: center;
        color: #aaa;
        font-size: 0.85em;
        flex: 1;
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .adv-card-table-wrap {
        overflow-x: auto;
        flex: 1;
    }

    .adv-card-table {
        width: 100%;
        border-collapse: collapse;
        font-size: 0.75em;
    }

    .adv-card-table th {
        text-align: left;
        padding: 4px 6px;
        border-bottom: 1px solid #ddd;
        background: #f4f4f4;
        font-weight: 600;
        white-space: nowrap;
    }

    .adv-card-table td {
        padding: 3px 6px;
        border-bottom: 1px solid #eee;
        white-space: nowrap;
    }

    .adv-card-table tbody tr:hover {
        background: #f8f8f8;
    }

    .adv-card-kv {
        display: flex;
        flex-direction: column;
        gap: 1px;
        padding: 6px;
        flex: 1;
    }

    .adv-kv-row {
        display: flex;
        justify-content: space-between;
        padding: 3px 6px;
        background: #fafafa;
        border-radius: 2px;
        font-size: 0.8em;
    }

    .adv-kv-key {
        color: #888;
        margin-right: 8px;
    }

    .adv-kv-val {
        color: #333;
        font-family: monospace;
        text-align: right;
        word-break: break-word;
    }

    /* ─── Expert Panel ──────────────────────────────────────────────────────── */

    .adv-expert-panel {
        background: white;
        border: 1px solid #ddd;
        border-radius: 6px;
        padding: 12px;
        margin-top: 4px;
    }

    .adv-expert-header {
        margin-bottom: 10px;
    }

    .adv-expert-header h4 {
        margin: 0 0 4px 0;
        font-size: 0.95em;
        color: #333;
    }

    .adv-expert-hint {
        font-size: 0.8em;
        color: #888;
    }

    .adv-expert-input {
        display: flex;
        gap: 8px;
        margin-bottom: 12px;
    }

    .adv-expert-input input {
        flex: 1;
        padding: 6px 10px;
        border: 1px solid #ccc;
        border-radius: 4px;
        font-family: monospace;
        font-size: 0.9em;
    }

    .adv-expert-input button {
        padding: 6px 16px;
        background: #006064;
        color: white;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85em;
    }

    .adv-expert-input button:disabled {
        background: #aaa;
        cursor: not-allowed;
    }

    .adv-expert-frames {
        display: flex;
        flex-direction: column;
        gap: 8px;
        margin-bottom: 10px;
    }

    .adv-expert-frame {
        border: 1px solid #eee;
        border-radius: 4px;
        overflow: hidden;
    }

    .adv-expert-frame-header {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 6px 10px;
        background: #f4f4f4;
        font-size: 0.85em;
    }

    .adv-badge {
        font-size: 0.8em;
        width: 18px;
        height: 18px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 50%;
        font-weight: bold;
    }

    .adv-badge.valid {
        background: #e6f4ea;
        color: #1e8e3e;
    }

    .adv-badge.invalid {
        background: #fce8e8;
        color: #c5221f;
    }

    .adv-frame-name {
        flex: 1;
        font-weight: 600;
        color: #333;
    }

    .adv-frame-meta {
        color: #888;
        font-size: 0.85em;
    }

    .adv-mini-table {
        width: 100%;
        border-collapse: collapse;
        font-size: 0.75em;
    }

    .adv-mini-table th, .adv-mini-table td {
        padding: 3px 6px;
        border-bottom: 1px solid #eee;
    }

    .adv-mini-table th {
        background: #f8f8f8;
        font-weight: 600;
    }

    .adv-mini-kv {
        padding: 6px;
        display: flex;
        flex-direction: column;
        gap: 2px;
        font-size: 0.8em;
    }

    .adv-raw-toggle {
        width: 100%;
        text-align: left;
        padding: 6px 10px;
        background: #f4f4f4;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.8em;
        color: #555;
    }

    .adv-raw-hex {
        padding: 8px;
        font-family: monospace;
        font-size: 0.75em;
        color: #333;
        word-break: break-all;
        background: #fafafa;
        border: 1px solid #eee;
        max-height: 150px;
        overflow-y: auto;
        margin-top: 4px;
    }
</style>
