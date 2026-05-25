<script lang="ts">
    import { onDestroy } from 'svelte';
    import { socketService } from '../../stores/socket';

    // ─── Props ──────────────────────────────────────────────────────────────
    let { size = 200 }: { size?: number } = $props();

    // ─── Constants ──────────────────────────────────────────────────────────
    const MAX_FORCE = 0.5;          // max speed (m/s)
    const MIN_SEND_INTERVAL = 200;  // ms — same cadence as PS4 controller
    const KEEP_ALIVE_INTERVAL = 200;// ms — repeat while stick is held
    const DEADZONE = 0.12;          // 12 % inner deadzone for cleaner center

    // ─── State ──────────────────────────────────────────────────────────────
    let canvas: HTMLCanvasElement | null = $state(null);
    let ctx: CanvasRenderingContext2D | null = null;
    let isDragging = $state(false);
    let stickX = $state(0);
    let stickY = $state(0);
    let linearSpeed = $state(0);
    let angularSpeed = $state(0);

    let lastSendTime = 0;
    let keepAliveTimer: ReturnType<typeof setInterval> | null = null;

    // ─── Geometry ───────────────────────────────────────────────────────────
    let centerX = $derived(size / 2);
    let centerY = $derived(size / 2);
    let maxRadius = $derived(size * 0.35);
    let stickRadius = $derived(size * 0.18);

    // ─── Canvas Drawing ─────────────────────────────────────────────────────
    function draw() {
        if (!ctx) return;
        ctx.clearRect(0, 0, size, size);

        // Base circle
        ctx.beginPath();
        ctx.arc(centerX, centerY, maxRadius, 0, Math.PI * 2);
        ctx.fillStyle = '#e8e8e8';
        ctx.fill();
        ctx.lineWidth = 2;
        ctx.strokeStyle = '#ccc';
        ctx.stroke();

        // Direction crosshair
        ctx.beginPath();
        ctx.moveTo(centerX, centerY - maxRadius);
        ctx.lineTo(centerX, centerY + maxRadius);
        ctx.moveTo(centerX - maxRadius, centerY);
        ctx.lineTo(centerX + maxRadius, centerY);
        ctx.strokeStyle = '#ddd';
        ctx.lineWidth = 1;
        ctx.stroke();

        // Forward arrow (↑ = forward)
        ctx.save();
        ctx.translate(centerX, centerY - maxRadius - 10);
        ctx.beginPath();
        ctx.moveTo(0, -7);
        ctx.lineTo(-5, 5);
        ctx.lineTo(5, 5);
        ctx.closePath();
        ctx.fillStyle = '#006064';
        ctx.fill();
        ctx.restore();

        // Stick handle
        const sx = centerX + stickX;
        const sy = centerY + stickY;

        // Stick shadow
        ctx.beginPath();
        ctx.arc(sx, sy + 2, stickRadius, 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(0,0,0,0.1)';
        ctx.fill();

        // Stick body
        ctx.beginPath();
        ctx.arc(sx, sy, stickRadius, 0, Math.PI * 2);
        const grad = ctx.createRadialGradient(
            sx - stickRadius * 0.3, sy - stickRadius * 0.3, stickRadius * 0.1,
            sx, sy, stickRadius
        );
        grad.addColorStop(0, '#00838f');
        grad.addColorStop(1, '#006064');
        ctx.fillStyle = grad;
        ctx.fill();
        ctx.strokeStyle = isDragging ? '#004d4f' : '#006064';
        ctx.lineWidth = 2;
        ctx.stroke();

        // Inner highlight
        ctx.beginPath();
        ctx.arc(sx - stickRadius * 0.2, sy - stickRadius * 0.2, stickRadius * 0.25, 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(255,255,255,0.25)';
        ctx.fill();
    }

    $effect(() => {
        if (canvas && !ctx) {
            ctx = canvas.getContext('2d');
            if (ctx) draw();
        }
        const _ = stickX;
        const __ = stickY;
        const ___ = isDragging;
        draw();
    });

    // ─── Physics ────────────────────────────────────────────────────────────
    // Grauonline / intuitive arcade-drive mapping.
    //
    //   UP    (ny < 0)  → linear  > 0  (forward)
    //   DOWN  (ny > 0)  → linear  < 0  (backward)
    //   LEFT  (nx < 0)  → turn left   (intuitive from user POV)
    //   RIGHT (nx > 0)  → turn right  (intuitive from user POV)
    //
    // The backend converts these intuitive values into PS4-compatible
    // values before calling manualDrive().
    function updateStick(clientX: number, clientY: number) {
        if (!canvas) return;
        const rect = canvas.getBoundingClientRect();
        const dx = clientX - rect.left - centerX;
        const dy = clientY - rect.top - centerY;
        const distance = Math.sqrt(dx * dx + dy * dy);
        const angle = Math.atan2(dy, dx);

        const clampedDist = Math.min(distance, maxRadius);
        stickX = Math.cos(angle) * clampedDist;
        stickY = Math.sin(angle) * clampedDist;

        // Normalised direction [-1, 1]
        const nx0 = stickX / maxRadius;   // -1 = left,  +1 = right
        const ny0 = stickY / maxRadius;   // -1 = up,    +1 = down
        const mag0 = Math.sqrt(nx0 * nx0 + ny0 * ny0);

        if (mag0 < DEADZONE) {
            linearSpeed = 0;
            angularSpeed = 0;
            return;
        }

        // Remove deadzone & re-normalise
        const scale = (mag0 - DEADZONE) / (1 - DEADZONE) / mag0;
        let nx = nx0 * scale;
        let ny = ny0 * scale;

        // Quadratic response curve for fine low-speed control
        const mag = Math.sqrt(nx * nx + ny * ny);
        const curvedMag = mag * mag;
        const curveScale = curvedMag / mag;
        nx *= curveScale;
        ny *= curveScale;

        // Standard differential-drive mapping (same convention as PS4 controller):
        //   linear  = -StickY  (up = forward, positive)
        //   angular = -StickX  (left = turn-left, positive)
        let lin = Math.round(-ny * MAX_FORCE * 100) / 100;
        let ang = Math.round(-nx * MAX_FORCE * 100) / 100;
        // Car-style steering: when driving backwards, invert angular so the
        // rear of the mower turns in the direction the stick is pushed.
        if (lin < 0) ang = -ang;
        // Avoid -0.00 display artifact
        if (Math.abs(lin) < 0.005) lin = 0;
        if (Math.abs(ang) < 0.005) ang = 0;
        linearSpeed = lin;
        angularSpeed = ang;
    }

    function sendNow(force = false) {
        const now = Date.now();
        if (!force && now - lastSendTime < MIN_SEND_INTERVAL) return;
        lastSendTime = now;
        socketService.sendJoystickMove(linearSpeed, angularSpeed);
    }

    function startKeepAlive() {
        stopKeepAlive();
        keepAliveTimer = setInterval(() => {
            if (isDragging && (linearSpeed !== 0 || angularSpeed !== 0)) {
                sendNow(true);
            }
        }, KEEP_ALIVE_INTERVAL);
    }

    function stopKeepAlive() {
        if (keepAliveTimer) {
            clearInterval(keepAliveTimer);
            keepAliveTimer = null;
        }
    }

    function formatLabel(name: string, val: number): string {
        // Pad positive values with a space so width stays constant in monospace
        const sign = val >= 0 ? '\u00A0' : '-';
        const absVal = Math.abs(val).toFixed(2);
        return `${name}: ${sign}${absVal}`;
    }

    function resetStick() {
        stickX = 0;
        stickY = 0;
        linearSpeed = 0;
        angularSpeed = 0;
    }

    // ─── Event Handlers ─────────────────────────────────────────────────────
    function onPointerDown(e: PointerEvent) {
        e.preventDefault();
        isDragging = true;
        if (canvas) canvas.setPointerCapture(e.pointerId);
        updateStick(e.clientX, e.clientY);
        sendNow(true);
        startKeepAlive();
    }

    function onPointerMove(e: PointerEvent) {
        if (!isDragging) return;
        e.preventDefault();
        updateStick(e.clientX, e.clientY);
        sendNow();
    }

    function onPointerUp(e: PointerEvent) {
        if (!isDragging) return;
        e.preventDefault();
        isDragging = false;
        stopKeepAlive();
        resetStick();
        sendNow(true);
    }

    function onPointerCancel(e: PointerEvent) {
        if (!isDragging) return;
        e.preventDefault();
        isDragging = false;
        stopKeepAlive();
        resetStick();
        sendNow(true);
    }

    // ─── Lifecycle ──────────────────────────────────────────────────────────
    onDestroy(() => {
        stopKeepAlive();
        if (isDragging) {
            resetStick();
            sendNow(true);
        }
    });
</script>

<div class="joystick-wrap">
    <canvas
        bind:this={canvas}
        width={size}
        height={size}
        class="joystick-canvas"
        class:active={isDragging}
        onpointerdown={onPointerDown}
        onpointermove={onPointerMove}
        onpointerup={onPointerUp}
        onpointercancel={onPointerCancel}
        role="application"
        aria-label="Joystick remote control"
    ></canvas>
    <div class="joystick-readout">
        <span class="joystick-label">{formatLabel('Lin', linearSpeed)}</span>
        <span class="joystick-label">{formatLabel('Ang', angularSpeed)}</span>
    </div>
</div>

<style lang="scss">
    .joystick-wrap {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 6px;
        user-select: none;
        touch-action: none;
    }

    .joystick-canvas {
        border-radius: 50%;
        cursor: pointer;
        touch-action: none;
        -webkit-user-select: none;
        user-select: none;
    }

    .joystick-canvas.active {
        cursor: grabbing;
    }

    .joystick-readout {
        display: flex;
        gap: 12px;
        font-size: 0.75em;
        font-family: monospace;
        color: #666;
    }

    .joystick-label {
        display: inline-block;
        min-width: 7.5ch;
        text-align: center;
        background: #f4f4f4;
        padding: 2px 8px;
        border-radius: 4px;
        border: 1px solid #ddd;
        font-variant-numeric: tabular-nums;
    }
</style>
