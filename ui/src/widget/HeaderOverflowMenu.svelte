<script lang="ts">
    import { browser } from '$app/environment';
    import IconOverflow from "carbon-icons-svelte/lib/OverflowMenuVertical.svelte";
    import IconLog from "carbon-icons-svelte/lib/DocumentView.svelte";
    import IconTerminal from "carbon-icons-svelte/lib/Terminal.svelte";
    import IconTest from "carbon-icons-svelte/lib/Tools.svelte";
    import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";
    import IconHelp from "carbon-icons-svelte/lib/Help.svelte";

    interface Props {
        onHelp?: () => void;
    }
    let { onHelp }: Props = $props();

    let open = $state(false);
    let menuRef: HTMLDivElement | null = null;

    const items = [
        { href: '/?dashboard=log',      icon: IconLog,      label: 'Log' },
        { href: '/?dashboard=terminal', icon: IconTerminal, label: 'Terminal' },
        { href: '/?dashboard=tests',    icon: IconTest,     label: 'Tests' },
        { href: '/settings',            icon: IconSettings, label: 'Settings' },
    ];

    function toggle(e: Event) {
        e.stopPropagation();
        open = !open;
    }

    function close() {
        open = false;
    }

    function onDocClick(e: MouseEvent) {
        if (menuRef && !menuRef.contains(e.target as Node)) {
            open = false;
        }
    }

    function onKey(e: KeyboardEvent) {
        if (e.key === 'Escape') open = false;
    }

    function clickHelp() {
        close();
        onHelp?.();
    }

    $effect(() => {
        if (!browser) return;
        if (open) {
            document.addEventListener('click', onDocClick);
            document.addEventListener('keydown', onKey);
        }
        return () => {
            document.removeEventListener('click', onDocClick);
            document.removeEventListener('keydown', onKey);
        };
    });
</script>

<div class="hom-wrapper" bind:this={menuRef}>
    <button class="hom-trigger"
            class:active={open}
            onclick={toggle}
            aria-label="More"
            title="More">
        <IconOverflow />
    </button>

    {#if open}
    <div class="hom-menu">
        {#each items as item}
            <a class="hom-item" href={item.href} onclick={close}>
                <item.icon />
                <span>{item.label}</span>
            </a>
        {/each}
        <div class="hom-divider"></div>
        <button class="hom-item" onclick={clickHelp}>
            <IconHelp />
            <span>Help</span>
        </button>
    </div>
    {/if}
</div>

<style lang="scss">
    .hom-wrapper {
        position: relative;
        display: flex;
        align-items: center;
        height: 100%;
    }

    .hom-trigger {
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

        &:hover,
        &.active {
            background: #333;
        }

        :global(svg) {
            width: 20px;
            height: 20px;
            fill: currentColor;
        }
    }

    .hom-menu {
        position: absolute;
        top: 48px;
        right: 0;
        min-width: 180px;
        background: white;
        border: 1px solid #ddd;
        border-radius: 0 0 8px 8px;
        box-shadow: 0 8px 24px rgba(0,0,0,0.15);
        z-index: 9999;
        padding: 4px 0;
        display: flex;
        flex-direction: column;
        animation: hom-slide 0.12s ease-out;
    }

    @keyframes hom-slide {
        from { opacity: 0; transform: translateY(-6px); }
        to   { opacity: 1; transform: translateY(0); }
    }

    .hom-item {
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 10px 16px;
        color: #333;
        text-decoration: none;
        font-size: 0.85em;
        cursor: pointer;
        background: transparent;
        border: none;
        width: 100%;
        text-align: left;
        transition: background 0.1s;

        &:hover {
            background: #f4f4f4;
        }

        :global(svg) {
            width: 18px;
            height: 18px;
            fill: #555;
            flex-shrink: 0;
        }
    }

    .hom-divider {
        height: 1px;
        background: #eee;
        margin: 4px 12px;
    }
</style>
