<script lang="ts">
  import { Button, Slider } from "carbon-components-svelte";
  import IconClear from "carbon-icons-svelte/lib/CloseOutline.svelte";
  import { Busy } from "../stores/busy";
    import { TextService } from "../text";
    import { Invalid } from "../stores/invalid";

  export let label: string;
  export let key: string;
  export let min: number;
  export let max: number;
  export let value: number;
  export let original: number;
  export let disabled: boolean = false;

  let dirty = false;
  $: dirty = value !== original;

  let labelMod = label;
  $: labelMod = dirty ? `${label} (*)` : label;

  let invalid = false;
  let invalidText = undefined;
  
  $: invalid = $Invalid === key;
  $: invalidText = !invalid ? undefined : TextService.invalidTextFor(key);

  function revert() {
    value = original;
  }
</script>

<main>
  <div style="flex: 1; display: flex; flex-direction: row;">
    <Slider labelText={labelMod} bind:value bind:min bind:max />
  </div>
  {#if dirty}
    <Button
      on:click={revert}
      disabled={disabled || $Busy}
      iconDescription="Revert changes"
      kind="ghost"
      icon={IconClear}
    />
  {/if}
</main>

<style lang="scss">
  main {
    display: flex;
    flex-direction: row;
  }
</style>
