<script lang="ts">
    import { onDestroy } from 'svelte';
    import { motorPlotStore, socketService, clearMotorPlotStore } from '../../stores/socket';

    // ─── Constants ──────────────────────────────────────────────────────────
    const MAX_POINTS = 600;

    // ─── State ──────────────────────────────────────────────────────────────
    let canvas: HTMLCanvasElement | null = $state(null);
    let chartWrap: HTMLDivElement | null = $state(null);
    let ctx: CanvasRenderingContext2D | null = null;
    let isRunning = $state(false);
    let dataPoints: { t: number; pwmL: number; pwmR: number; pwmMow: number; ticksL: number; ticksR: number; ticksMow: number }[] = $state([]);
    let startTime = 0;
    let lastProcessedIndex = 0;
    let unsubStore: (() => void) | null = null;
    let stopTimer: ReturnType<typeof setTimeout> | null = null;
    let ackWarning = $state(false);

    // ─── Colors ─────────────────────────────────────────────────────────────
    const COLORS = {
        pwmL:   '#e63946',
        pwmR:   '#457b9d',
        pwmMow: '#2a9d8f',
        ticksL: '#f4a261',
        ticksR: '#a8dadc',
        ticksMow:'#e9c46a',
    };

    // ─── Responsive canvas sizing ───────────────────────────────────────────
    function resizeCanvas() {
        if (!canvas || !chartWrap) return;
        const dpr = window.devicePixelRatio || 1;
        const rect = chartWrap.getBoundingClientRect();
        // Leave room for padding inside the wrapper
        const w = Math.max(200, Math.floor(rect.width));
        const h = Math.max(150, Math.floor(rect.height));

        if (canvas.width !== Math.floor(w * dpr) || canvas.height !== Math.floor(h * dpr)) {
            canvas.width = Math.floor(w * dpr);
            canvas.height = Math.floor(h * dpr);
            canvas.style.width = w + 'px';
            canvas.style.height = h + 'px';
            ctx = canvas.getContext('2d');
            if (ctx) ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            draw();
        }
    }

    $effect(() => {
        if (chartWrap && canvas) {
            resizeCanvas();
            const ro = new ResizeObserver(() => resizeCanvas());
            ro.observe(chartWrap);
            window.addEventListener('resize', resizeCanvas);
            return () => {
                ro.disconnect();
                window.removeEventListener('resize', resizeCanvas);
            };
        }
    });

    // ─── Data Parsing ───────────────────────────────────────────────────────
    function parseLine(text: string) {
        if (text.includes('pwmLeft') || text.includes('motor plot') || text.includes('Serial Plotter') || text.startsWith('CON:')) {
            return;
        }
        const parts = text.split(',').map(p => parseFloat(p.trim())).filter(n => !isNaN(n));
        if (parts.length < 6) return;

        // Sunray motor.cpp: 300+pwmLeft, 300+pwmRight, pwmMow, 300+ticksLeft, 300+ticksRight, ticksMow
        const pwmL   = parts[0] - 300;
        const pwmR   = parts[1] - 300;
        const pwmMow = parts[2];
        const ticksL = parts[3] - 300;
        const ticksR = parts[4] - 300;
        const ticksMow = parts[5];

        const pts = dataPoints;
        pts.push({ t: Date.now() - startTime, pwmL, pwmR, pwmMow, ticksL, ticksR, ticksMow });
        if (pts.length > MAX_POINTS) pts.shift();
        dataPoints = pts;

        requestAnimationFrame(() => {
            initCtx();
            draw();
        });
    }

    // ─── Store subscription ─────────────────────────────────────────────────
    function startStoreSubscription() {
        if (unsubStore) return;
        unsubStore = motorPlotStore.subscribe((lines) => {
            if (!isRunning) return;
            if (!lines || lines.length === 0) {
                lastProcessedIndex = 0;
                return;
            }
            let newCount = 0;
            for (let i = lastProcessedIndex; i < lines.length; i++) {
                parseLine(lines[i].text);
                newCount++;
            }
            lastProcessedIndex = lines.length;

        });
    }

    function stopStoreSubscription() {
        if (unsubStore) {
            unsubStore();
            unsubStore = null;
        }
    }

    // ─── Canvas init & draw ─────────────────────────────────────────────────
    function initCtx() {
        if (!ctx && canvas) {
            const dpr = window.devicePixelRatio || 1;
            ctx = canvas.getContext('2d');
            if (ctx) ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

        }
    }

    function draw() {
        if (!ctx || !canvas) return;
        if (dataPoints.length === 0) {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            return;
        }

        const cssW = canvas.clientWidth;
        const cssH = canvas.clientHeight;
        const pad = { top: 36, right: 60, bottom: 30, left: 50 };
        const chartW = cssW - pad.left - pad.right;
        const chartH = cssH - pad.top - pad.bottom;

        ctx.clearRect(0, 0, cssW, cssH);

        // ── Y-Axis Scales ──
        const pwmMax = Math.max(255, ...dataPoints.map(d => Math.max(Math.abs(d.pwmL), Math.abs(d.pwmR), Math.abs(d.pwmMow))));
        const ticksMax = Math.max(50, ...dataPoints.map(d => Math.max(d.ticksL, d.ticksR, d.ticksMow)));

        // PWM: 0 in der Mitte, Ticks: 0 unten
        const zeroY = pad.top + chartH / 2;

        const series = [
            { key: 'pwmL',   color: COLORS.pwmL,   scaleY: pwmMax,   isPwm: true,  offset: -8, dash: [5, 3],   width: 2,   label: 'PWM L' },
            { key: 'pwmR',   color: COLORS.pwmR,   scaleY: pwmMax,   isPwm: true,  offset: 0,  dash: [2, 3],   width: 2,   label: 'PWM R' },
            { key: 'pwmMow', color: COLORS.pwmMow, scaleY: pwmMax,   isPwm: true,  offset: 8,  dash: [],       width: 2,   label: 'PWM Mow' },
            { key: 'ticksL', color: COLORS.ticksL, scaleY: ticksMax, isPwm: false, offset: 0,  dash: [5, 3],   width: 1.5, label: 'Ticks L' },
            { key: 'ticksR', color: COLORS.ticksR, scaleY: ticksMax, isPwm: false, offset: 0,  dash: [2, 3],   width: 1.5, label: 'Ticks R' },
            { key: 'ticksMow',color: COLORS.ticksMow,scaleY: ticksMax,isPwm: false, offset: 0,  dash: [],       width: 1.5, label: 'Ticks Mow' },
        ];

        // ── Grid ──
        ctx.strokeStyle = '#e0e0e0';
        ctx.lineWidth = 1;
        for (let i = 0; i <= 5; i++) {
            const y = pad.top + (chartH / 5) * i;
            ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(cssW - pad.right, y); ctx.stroke();
        }

        // ── Zero line (PWM) ──
        ctx.strokeStyle = '#bbb';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath(); ctx.moveTo(pad.left, zeroY); ctx.lineTo(cssW - pad.right, zeroY); ctx.stroke();
        ctx.setLineDash([]);

        // ── Data lines ──
        for (const s of series) {
            ctx.strokeStyle = s.color;
            ctx.lineWidth = s.width;
            ctx.setLineDash(s.dash);
            ctx.beginPath();
            for (let i = 0; i < dataPoints.length; i++) {
                const x = pad.left + (i / (MAX_POINTS - 1)) * chartW;
                const val = dataPoints[i][s.key as keyof typeof dataPoints[0]] as number;
                let y: number;
                if (s.isPwm) {
                    // PWM: 0 in der Mitte, positiv nach oben, negativ nach unten
                    // kleiner vertikaler Offset pro Serie, damit sie sich nicht überlappen
                    y = zeroY + s.offset - (val / s.scaleY) * (chartH / 2);
                } else {
                    // Ticks: 0 unten, positiv nach oben
                    y = pad.top + chartH - (val / s.scaleY) * chartH;
                }
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }
        ctx.setLineDash([]);

        // ── Left Y-axis labels (PWM) ──
        ctx.fillStyle = '#666';
        ctx.font = '11px monospace';
        ctx.textAlign = 'right';
        for (let i = 0; i <= 4; i++) {
            const frac = 1 - i / 4;
            const val = Math.round((frac * 2 - 1) * pwmMax);
            const y = pad.top + (chartH / 4) * i;
            ctx.fillText(String(val), pad.left - 6, y + 4);
        }

        // ── Right Y-axis labels (Ticks) ──
        ctx.textAlign = 'left';
        for (let i = 0; i <= 4; i++) {
            const val = Math.round((1 - i / 4) * ticksMax);
            const y = pad.top + (chartH / 4) * i;
            ctx.fillText(String(val), cssW - pad.right + 6, y + 4);
        }

        // ── Legend ──
        ctx.textAlign = 'left';
        let lx = pad.left;
        for (const s of series) {
            ctx.fillStyle = s.color;
            ctx.fillRect(lx, 10, 14, 3);
            ctx.fillStyle = '#333';
            ctx.font = 'bold 11px sans-serif';
            ctx.fillText(s.label, lx + 18, 16);
            lx += ctx.measureText(s.label).width + 28;
        }

        // ── Axis titles ──
        ctx.fillStyle = '#888';
        ctx.font = '10px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(`${Math.round(dataPoints.length / 10)}s / 60s`, cssW / 2, cssH - 8);

        // ── Scale labels ──
        ctx.save();
        ctx.translate(12, cssH / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillStyle = '#e63946';
        ctx.font = 'bold 11px sans-serif';
        ctx.fillText('PWM', 0, 0);
        ctx.restore();

        ctx.save();
        ctx.translate(cssW - 12, cssH / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillStyle = '#f4a261';
        ctx.font = 'bold 11px sans-serif';
        ctx.fillText('Ticks', 0, 0);
        ctx.restore();
    }

    // ─── Controls ───────────────────────────────────────────────────────────
    function startPlot() {
        if (isRunning || !ackWarning) return;

        isRunning = true;
        dataPoints = [];
        lastProcessedIndex = 0;
        startTime = Date.now();
        clearMotorPlotStore();


        socketService.sendConsoleCommand('AT+Q');
        startStoreSubscription();

        stopTimer = setTimeout(() => stopPlot(), 65000);
    }

    function stopPlot() {
        isRunning = false;
        stopStoreSubscription();
        if (stopTimer) { clearTimeout(stopTimer); stopTimer = null; }
    }

    function clearPlot() {
        dataPoints = [];
        lastProcessedIndex = 0;
        clearMotorPlotStore();
        if (ctx && canvas) ctx.clearRect(0, 0, canvas.width, canvas.height);
    }

    onDestroy(() => stopPlot());
</script>

<div class="motor-plot">
    <div class="mp-header">
        <h3>🔧 Motor Plot (AT+Q)</h3>
        <p class="mp-desc">
            60-Sekunden-Motortest. Motoren beschleunigen auf Volllast und bremsen wieder ab.
            <strong>Links</strong> = PWM (±255), <strong>Rechts</strong> = Ticks.
        </p>
        <div class="mp-warning">
            ⚠ <strong>Verletzungsgefahr!</strong> Beim Start dieses Tests laufen die
            Fahrmotoren <strong>und das Mähwerk</strong> mit voller Geschwindigkeit an.
            Mäher vorher sicher aufbocken – Finger und Hände fernhalten!
            <label class="mp-ack">
                <input type="checkbox" bind:checked={ackWarning} />
                <span>Verstanden – Sicherheitsvorkehrungen getroffen</span>
            </label>
        </div>
        <div class="mp-actions">
            <button class="mp-btn start" onclick={startPlot} disabled={isRunning || !ackWarning}>
                {isRunning ? '⏳ Läuft...' : '▶ Start'}
            </button>
            <button class="mp-btn stop" onclick={stopPlot} disabled={!isRunning}>
                ⏹ Stop
            </button>
            <button class="mp-btn clear" onclick={clearPlot}>
                🗑 Clear
            </button>
        </div>
    </div>

    <div class="mp-chart-wrap" bind:this={chartWrap}>
        <canvas bind:this={canvas} class="mp-canvas"></canvas>
    </div>

    <div class="mp-stats">
        <div class="mp-stat">
            <span class="mp-stat-label">Datenpunkte</span>
            <span class="mp-stat-val">{dataPoints.length} / {MAX_POINTS}</span>
        </div>
        <div class="mp-stat">
            <span class="mp-stat-label">Dauer</span>
            <span class="mp-stat-val">{dataPoints.length > 0 ? (dataPoints[dataPoints.length-1].t / 1000).toFixed(1) : '0.0'}s</span>
        </div>
        {#if dataPoints.length > 0}
            {@const last = dataPoints[dataPoints.length - 1]}
            <div class="mp-stat">
                <span class="mp-stat-label">PWM L/R/Mow</span>
                <span class="mp-stat-val">{last.pwmL.toFixed(0)} / {last.pwmR.toFixed(0)} / {last.pwmMow.toFixed(0)}</span>
            </div>
            <div class="mp-stat">
                <span class="mp-stat-label">Ticks L/R/Mow</span>
                <span class="mp-stat-val">{last.ticksL.toFixed(0)} / {last.ticksR.toFixed(0)} / {last.ticksMow.toFixed(0)}</span>
            </div>
        {/if}
    </div>
</div>

<style lang="scss">
    .motor-plot {
        display: flex;
        flex-direction: column;
        gap: 12px;
        background: white;
        border-radius: 8px;
        border: 1px solid #ddd;
        padding: 16px;
        flex: 1;
        min-height: 0;
    }

    .mp-header h3 { margin: 0 0 4px 0; font-size: 1.1em; }
    .mp-desc { margin: 0 0 10px 0; font-size: 0.85em; color: #666; }
    .mp-actions { display: flex; gap: 8px; margin-top: 12px; }

    .mp-ack {
        display: flex;
        align-items: center;
        gap: 8px;
        margin-top: 10px;
        cursor: pointer;
        font-weight: 600;

        input[type="checkbox"] {
            width: 18px;
            height: 18px;
            accent-color: #c5221f;
            cursor: pointer;
        }
    }

    .mp-warning {
        background: #fff3cd;
        border: 1px solid #ffc107;
        border-radius: 4px;
        padding: 10px 14px;
        font-size: 0.85em;
        color: #664d03;
        line-height: 1.5;
    }

    .mp-btn {
        padding: 6px 16px;
        border: 1px solid #ccc;
        border-radius: 4px;
        background: #f4f4f4;
        cursor: pointer;
        font-size: 0.9em;
        transition: background 0.15s;

        &:hover:not(:disabled) { background: #e0e0e0; }
        &:disabled { opacity: 0.5; cursor: not-allowed; }

        &.start { background: #006064; color: white; border-color: #006064;
            &:hover:not(:disabled) { background: #004d4f; } }
        &.stop  { background: #c5221f; color: white; border-color: #c5221f;
            &:hover:not(:disabled) { background: #a11b18; } }
    }

    .mp-chart-wrap {
        flex: 1;
        min-height: 0;
        width: 100%;
        background: #fafafa;
        border-radius: 4px;
        border: 1px solid #eee;
        position: relative;
    }
    .mp-canvas {
        position: absolute;
        top: 0; left: 0;
        width: 100%; height: 100%;
        display: block;
    }

    .mp-stats { display: flex; flex-wrap: wrap; gap: 12px; }
    .mp-stat {
        background: #f8f8f8; border: 1px solid #eee; border-radius: 4px;
        padding: 6px 12px; display: flex; flex-direction: column; gap: 2px;
    }
    .mp-stat-label { font-size: 0.75em; color: #888; }
    .mp-stat-val { font-size: 0.9em; font-family: monospace; color: #333; }
</style>
