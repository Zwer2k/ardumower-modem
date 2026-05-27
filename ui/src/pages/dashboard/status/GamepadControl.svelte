<script lang="ts">
    import { onDestroy } from 'svelte';
    import { socketService } from '../../../stores/socket';

    // ─── Constants ──────────────────────────────────────────────────────────
    const MAX_FORCE = 0.5;
    const SEND_INTERVAL = 200;
    const DEADZONE = 0.12;

    // ─── State ──────────────────────────────────────────────────────────────
    let connected = $state(false);
    let gamepadIndex: number | null = null;
    let linearSpeed = $state(0);
    let angularSpeed = $state(0);
    let lastSendTime = 0;
    let rafId: number | null = null;
    let lastButtonState = new Map<number, boolean>();
    let paused = false;

    // ─── Gamepad detection ──────────────────────────────────────────────────
    function onGamepadConnected(e: GamepadEvent) {
        const gp = e.gamepad;
        connected = true;
        gamepadIndex = gp.index;
        startLoop();
    }

    function onGamepadDisconnected(e: GamepadEvent) {
        if (e.gamepad.index === gamepadIndex) {
            connected = false;
            gamepadIndex = null;
            paused = false;
            stopLoop();
            sendZero();
        }
    }

    window.addEventListener('gamepadconnected', onGamepadConnected);
    window.addEventListener('gamepaddisconnected', onGamepadDisconnected);

    // Check for already-connected gamepads
    const existing = navigator.getGamepads();
    for (let i = 0; i < existing.length; i++) {
        const gp = existing[i];
        if (gp) {
            connected = true;
            gamepadIndex = gp.index;
            startLoop();
            break;
        }
    }





    // ─── Main loop ──────────────────────────────────────────────────────────
    function startLoop() {
        if (rafId) return;
        loop();
    }

    function stopLoop() {
        if (rafId) {
            cancelAnimationFrame(rafId);
            rafId = null;
        }
    }

    function loop() {
        rafId = requestAnimationFrame(loop);

        // If we have a known gamepad, read it
        if (gamepadIndex !== null) {
            const gp = navigator.getGamepads()[gamepadIndex];
            if (!gp) {
                // Controller was unplugged without firing disconnect event
                connected = false;
                gamepadIndex = null;
                paused = false;
                sendZero();
                return;
            }
            readSticks(gp);
            readButtons(gp);
            return;
        }

        // No known gamepad: scan for newly active ones
        if (!connected) {
            const pads = navigator.getGamepads();
            for (let i = 0; i < pads.length; i++) {
                const gp = pads[i];
                if (gp) {
                    // A gamepad is present. Check if any button or stick is active
                    // to auto-reconnect after PS-button pause or initial connection
                    let active = false;
                    for (const btn of gp.buttons) {
                        if (btn.pressed) { active = true; break; }
                    }
                    if (!active) {
                        for (const axis of gp.axes) {
                            if (Math.abs(axis) > 0.5) { active = true; break; }
                        }
                    }
                    if (active) {
                        connected = true;
                        gamepadIndex = gp.index;
                        paused = false;
                        lastButtonState.clear();
                    }
                }
            }
        }
    }

    // ─── Stick reading ──────────────────────────────────────────────────────
    function readSticks(gp: Gamepad) {
        // Left stick (axes 0,1): half sensitivity
        // Right stick (axes 2,3): quarter sensitivity
        let nx0 = gp.axes[0] ?? 0;
        let ny0 = gp.axes[1] ?? 0;
        let sensitivity = 0.5;

        const leftMag = Math.sqrt(nx0 * nx0 + ny0 * ny0);
        if (leftMag < DEADZONE && gp.axes.length >= 4) {
            nx0 = gp.axes[2] ?? 0;
            ny0 = gp.axes[3] ?? 0;
            sensitivity = 0.25;
        }

        const mag0 = Math.sqrt(nx0 * nx0 + ny0 * ny0);
        if (mag0 < DEADZONE) {
            if (linearSpeed !== 0 || angularSpeed !== 0) {
                linearSpeed = 0;
                angularSpeed = 0;
                sendNow(true);
            }
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

        // Apply stick sensitivity AFTER curve
        nx *= sensitivity;
        ny *= sensitivity;

        let lin = Math.round(-ny * MAX_FORCE * 100) / 100;
        let ang = Math.round(-nx * MAX_FORCE * 100) / 100;
        if (lin < 0) ang = -ang;
        if (Math.abs(lin) < 0.005) lin = 0;
        if (Math.abs(ang) < 0.005) ang = 0;

        linearSpeed = lin;
        angularSpeed = ang;
        sendNow();
    }

    // ─── Button reading ─────────────────────────────────────────────────────
    function readButtons(gp: Gamepad) {
        const now = Date.now();

        const buttons = gp.buttons;
        if (!buttons.length) return;

        // PS button (index 16 on DS4) = pause / resume
        if (buttons.length > 16) {
            const psPressed = buttons[16].pressed;
            const wasPsPressed = lastButtonState.get(16) ?? false;
            if (psPressed && !wasPsPressed) {
                paused = !paused;
                if (paused) {
                    sendZero();
                }
            }
            lastButtonState.set(16, psPressed);
        }

        if (paused) return;

        if (now - lastSendTime < SEND_INTERVAL) return;

        for (let i = 0; i < Math.min(buttons.length, 4); i++) {
            const pressed = buttons[i].pressed;
            const wasPressed = lastButtonState.get(i) ?? false;

            if (pressed && !wasPressed) {
                handleButton(i);
                lastSendTime = now;
            }
            lastButtonState.set(i, pressed);
        }
    }

    function handleButton(index: number) {
        switch (index) {
            case 0: // Cross/A
                break;
            case 1: // Circle/B
                break;
            case 2: // Square/X
                break;
            case 3: // Triangle/Y
                break;
        }
    }

    // ─── Send ───────────────────────────────────────────────────────────────
    function sendNow(force = false) {
        const now = Date.now();
        if (!force && now - lastSendTime < SEND_INTERVAL) return;
        lastSendTime = now;
        socketService.sendJoystickMove(linearSpeed, angularSpeed);
    }

    function sendZero() {
        linearSpeed = 0;
        angularSpeed = 0;
        socketService.sendJoystickMove(0, 0);
    }

    onDestroy(() => {
        stopLoop();
        window.removeEventListener('gamepadconnected', onGamepadConnected);
        window.removeEventListener('gamepaddisconnected', onGamepadDisconnected);
        sendZero();
    });
</script>

<div class="gamepad-control">
    <div class="gp-status">
        <span class="gp-led" class:on={connected}></span>
        <span class="gp-label-status">
            {connected ? '🎮 Controller verbunden' : 'Kein Controller'}
        </span>
    </div>

    {#if connected}
        <div class="gp-readout">
            <span class="gp-val">Lin: {linearSpeed.toFixed(2)}</span>
            <span class="gp-val">Ang: {angularSpeed.toFixed(2)}</span>
        </div>
    {:else}
        <p class="gp-hint">
            Controller per USB/Bluetooth koppeln, dann einen Knopf drücken.
            PS-Knopf = Steuerung pausieren.
        </p>
    {/if}
</div>

<style lang="scss">
    .gamepad-control {
        width: 100%;
        padding: 8px 0 0 0;
        font-size: 0.85em;
        box-sizing: border-box;
    }

    .gp-status {
        display: flex;
        align-items: center;
        gap: 8px;
        font-weight: 600;
        color: #333;
    }

    .gp-led {
        width: 10px;
        height: 10px;
        border-radius: 50%;
        background: #ccc;
        transition: background 0.3s;

        &.on {
            background: #2a9d8f;
            box-shadow: 0 0 6px #2a9d8f;
        }
    }

    .gp-readout {
        display: flex;
        gap: 8px;
        margin-top: 6px;
        font-family: monospace;
        font-size: 0.9em;
    }

    .gp-val {
        display: inline-block;
        min-width: 7ch;
        text-align: center;
        background: #f4f4f4;
        padding: 2px 6px;
        border-radius: 4px;
        border: 1px solid #ddd;
        font-variant-numeric: tabular-nums;
    }

    .gp-hint {
        margin: 6px 0 0 0;
        font-size: 0.8em;
        color: #888;
        line-height: 1.4;
        word-break: break-word;
    }

    .gp-btn {
        margin-top: 8px;
        padding: 5px 10px;
        border: 1px solid #ccc;
        border-radius: 4px;
        font-size: 0.8em;
        cursor: pointer;
        transition: background 0.15s;
        white-space: nowrap;

        &.connect {
            background: #e8f5e9;
            border-color: #4caf50;
            color: #2e7d32;
            &:hover { background: #c8e6c9; }
        }

        &.disconnect {
            background: #ffebee;
            border-color: #ef5350;
            color: #c62828;
            &:hover { background: #ffcdd2; }
        }
    }
</style>
