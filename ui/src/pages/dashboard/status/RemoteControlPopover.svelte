<script lang="ts">
    import { browser } from '$app/environment';
    import { socketStore } from '../../../stores/socket';
    import Joystick from './Joystick.svelte';
    import GamepadControl from './GamepadControl.svelte';
    import ControlButtons from './ControlButtons.svelte';
    import IconJoystick from "carbon-icons-svelte/lib/GameConsole.svelte";

    let open = $state(false);
    let wrapperRef: HTMLDivElement | null = null;

    function toggle() {
        open = !open;
    }

    function close() {
        open = false;
    }

    // Close when clicking outside the wrapper
    function onDocClick(e: MouseEvent) {
        if (!open) return;
        if (wrapperRef && !wrapperRef.contains(e.target as Node)) {
            open = false;
        }
    }

    // Close on Escape key
    function onKeyDown(e: KeyboardEvent) {
        if (e.key === 'Escape' && open) {
            open = false;
        }
    }

    $effect(() => {
        if (!browser) return;
        document.addEventListener('click', onDocClick);
        document.addEventListener('keydown', onKeyDown);
        return () => {
            document.removeEventListener('click', onDocClick);
            document.removeEventListener('keydown', onKeyDown);
        };
    });
</script>

{#if browser}
<div class="rc-wrapper" bind:this={wrapperRef}>
    <button class="rc-trigger"
            class:active={open}
            onclick={toggle}
            aria-label="Remote Control"
            title="Remote Control">
        <IconJoystick />
    </button>

    {#if open}
    <div class="rc-popover">
        <div class="rc-popover-header">
            <span class="rc-title">🎮 Remote Control</span>
            <button class="rc-close" onclick={close} aria-label="Close">✕</button>
        </div>
        <div class="rc-popover-body">
            <div class="rc-joystick-col">
                <Joystick size={180} />
                <GamepadControl />
            </div>
            <div class="rc-divider"></div>
            <div class="rc-controls-col">
                <ControlButtons desiredState={$socketStore.desiredState} />
            </div>
        </div>
    </div>
    {/if}
</div>
{/if}

<style lang="scss">
    .rc-wrapper {
        position: relative;
        display: flex;
        align-items: center;
        height: 100%;
    }

    .rc-trigger {
        display: flex;
        align-items: center;
        justify-content: center;
        width: 48px;
        height: 48px;
        background: transparent;
        border: none;
        color: #f4f4f4;
        cursor: pointer;
        transition: background 0.15s;
        padding: 0;
    }

    .rc-trigger:hover,
    .rc-trigger.active {
        background: #333;
    }

    .rc-trigger :global(svg) {
        width: 20px;
        height: 20px;
        fill: currentColor;
    }

    .rc-popover {
        position: absolute;
        top: 48px;
        right: 0;
        width: 520px;
        background: white;
        border: 1px solid #ddd;
        border-radius: 0 0 8px 8px;
        box-shadow: 0 8px 24px rgba(0,0,0,0.15);
        z-index: 9999;
        padding: 16px;
        display: flex;
        flex-direction: column;
        gap: 12px;
        animation: rc-slide-in 0.15s ease-out;
    }

    @keyframes rc-slide-in {
        from { opacity: 0; transform: translateY(-8px); }
        to   { opacity: 1; transform: translateY(0); }
    }

    .rc-popover-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        border-bottom: 1px solid #eee;
        padding-bottom: 8px;
    }

    .rc-title {
        font-size: 0.95em;
        font-weight: 600;
        color: #333;
    }

    .rc-close {
        width: 28px;
        height: 28px;
        display: flex;
        align-items: center;
        justify-content: center;
        background: transparent;
        border: none;
        border-radius: 4px;
        color: #888;
        cursor: pointer;
        font-size: 1em;
        line-height: 1;
        transition: background 0.15s, color 0.15s;

        &:hover {
            background: #f4f4f4;
            color: #333;
        }
    }

    .rc-popover-body {
        display: flex;
        gap: 16px;
        align-items: flex-start;
    }

    .rc-joystick-col {
        flex: 0 0 200px;
        width: 200px;
        display: flex;
        flex-direction: column;
        align-items: center;
    }

    .rc-divider {
        width: 1px;
        background: #ddd;
        align-self: stretch;
        min-height: 180px;
    }

    .rc-controls-col {
        flex: 1;
        min-width: 0;
    }

    /* ─── Smartphone / narrow screens ────────────────────────────────────── */
    @media (max-width: 640px) {
        .rc-popover {
            position: fixed;
            top: 48px;
            left: 0;
            right: 0;
            width: 100vw;
            height: calc(100vh - 48px);
            border-radius: 0;
            border: none;
            border-top: 1px solid #ddd;
            box-shadow: none;
            padding: 12px;
            justify-content: flex-start;
            overflow-y: auto;
        }

        .rc-popover-body {
            flex-direction: column;
            align-items: center;
            gap: 12px;
        }

        .rc-divider {
            display: none;
        }

        .rc-joystick-col {
            width: 100%;
            align-items: center;
        }

        .rc-controls-col {
            width: 100%;
        }
    }
</style>
