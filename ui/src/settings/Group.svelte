<script lang="ts">
  import { slide } from "svelte/transition";
  import { AccordionItem } from "carbon-components-svelte";

  export let title: string;
  export let settings: any = {};
  export let original: any = {};
  export let dirty = false;
  export let open = true;
  export let enabledKey = "enabled";
  $: dirty = JSON.stringify(settings) !== JSON.stringify(original);

  let titleMod = title;
  $: titleMod = dirty ? `${title} (*)` : title;
  
  // Stabile Transition-Parameter
  const slideParams = {
    duration: 250,
    axis: 'y' as const
  };
</script>

<AccordionItem title={titleMod} {open}>
  <slot />
  {#if settings[enabledKey]}
    <div 
      transition:slide={slideParams}
      class="enabled-content"
    >
      <slot name="enabled" />
    </div>
  {/if}
</AccordionItem>

<style>
  .enabled-content {
    /* Layout-Stabilisierung */
    overflow: hidden;
    contain: layout style;
    
    /* Hardware-Acceleration für bessere Performance */
    transform: translateZ(0);
    backface-visibility: hidden;
  }
</style>
