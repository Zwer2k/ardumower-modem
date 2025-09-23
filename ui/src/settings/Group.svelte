<script lang="ts">
  import { slide } from "svelte/transition";
  import { AccordionItem } from "carbon-components-svelte";
  import { tick, onMount } from 'svelte';

  export let title: string;
  export let settings: any = {};
  export let original: any = {};
  export let dirty = false;
  export let open = true;
  export let enabledKey = "enabled";
  $: dirty = JSON.stringify(settings) !== JSON.stringify(original);

  let titleMod = title;
  $: titleMod = dirty ? `${title} (*)` : title;
  
  // Verwende eine einfachere, stabilere Transition
  let contentDiv: HTMLDivElement;
  let showContent = false;
  
  // Reaktiv auf settings[enabledKey] ohne sofortige DOM-Änderung
  $: {
    if (settings[enabledKey] !== showContent) {
      updateContentVisibility(settings[enabledKey]);
    }
  }
  
  async function updateContentVisibility(enabled: boolean) {
    if (enabled && !showContent) {
      showContent = true;
    } else if (!enabled && showContent) {
      // Kurze Verzögerung für sanftere Transition
      await new Promise(resolve => setTimeout(resolve, 100));
      showContent = false;
    }
  }
</script>

<AccordionItem title={titleMod} {open}>
  <slot />
  {#if showContent}
    <div 
      bind:this={contentDiv}
      class="enabled-content"
      style="animation: slideDown 0.25s ease-out;"
    >
      <slot name="enabled" />
    </div>
  {/if}
</AccordionItem>

<style>
  .enabled-content {
    /* Layout-Stabilisierung */
    overflow: hidden;
    contain: layout style size;
    
    /* Verhindere Reflow während Animation */
    will-change: height;
    transform: translateZ(0);
    backface-visibility: hidden;
  }
  
  @keyframes slideDown {
    from {
      max-height: 0;
      opacity: 0;
    }
    to {
      max-height: 1000px;
      opacity: 1;
    }
  }
  
  /* Spezielle Behandlung für ausgehende Animation */
  :global(.enabled-content:not(.visible)) {
    animation: slideUp 0.25s ease-in forwards;
  }
  
  @keyframes slideUp {
    from {
      max-height: 1000px;
      opacity: 1;
    }
    to {
      max-height: 0;
      opacity: 0;
    }
  }
</style>
