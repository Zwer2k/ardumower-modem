<script lang="ts">
  import { Button } from "carbon-components-svelte";
  import IconClear from "carbon-icons-svelte/lib/CloseOutline.svelte";
  import { Busy } from "../stores/busy";
  import { Invalid } from "../stores/invalid";
  import { TextService } from "../text";
  import { ipInput } from "../actions/ipInput";

  export let label: string;
  export let key: string;
  export let helpText: string = "";
  export let helpTextEmpty: string = "";
  export let helpTextNotEmpty: string = "";
  export let value: any;
  export let original = value;
  export let disabled: boolean = false;
  export let readonly: boolean = false;
  export let required: boolean = true;

  let dirty = false;
  $: dirty = value !== original;

  let labelMod = label;
  $: labelMod = dirty ? `${label} (*)` : label;

  let help: string;
  $: help =
    !!value && value.toString().length > 0
      ? !!helpTextNotEmpty && helpTextNotEmpty.length > 0
        ? helpTextNotEmpty
        : helpText
      : !!helpTextEmpty && helpTextEmpty.length > 0
      ? helpTextEmpty
      : helpText;

  let invalid = false;
  let invalidText = undefined;
  $: invalid = $Invalid === key;
  $: invalidText = !invalid ? undefined : TextService.invalidTextFor(key);

  function revert() {
    value = original;
    Invalid.set("");
  }
</script>

<main>
  <div class="ip-field-wrapper" class:invalid class:disabled={disabled || $Busy}>
    <label class="bx--label" for={key} class:bx--label--disabled={disabled || $Busy}>
      {labelMod}
    </label>
    <input
      id={key}
      class="bx--text-input"
      class:bx--text-input--invalid={invalid}
      type="text"
      inputmode="numeric"
      bind:value
      {readonly}
      disabled={disabled || $Busy}
      {required}
      use:ipInput
      placeholder="0.0.0.0"
      on:input
    />
    {#if invalid}
      <div class="bx--form-requirement">{invalidText}</div>
    {/if}
    {#if help && !invalid}
      <div class="bx--form__helper-text">{help}</div>
    {/if}
  </div>
  {#if dirty}
    <Button
      on:click={revert}
      disabled={$Busy}
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
  .ip-field-wrapper {
    flex: 1;
  }
  .invalid {
    input {
      outline: 2px solid #da1e28;
      outline-offset: -2px;
    }
  }
  input {
    width: 100%;
  }
  .disabled {
    opacity: 0.5;
    cursor: not-allowed;
    input {
      cursor: not-allowed;
    }
  }
</style>
