<script lang="ts">
  export let title: string;
  export let settings: any = {};
  export let original: any = {};
  export let dirty = false;
  export let open = true;
  export let enabledKey = "enabled";
  $: dirty = JSON.stringify(settings) !== JSON.stringify(original);

  let titleMod = title;
  $: titleMod = dirty ? `${title} (*)` : title;

  let isOpen = open;
  function toggle() {
    isOpen = !isOpen;
  }
</script>

<div class="group">
  <button class="group-header" on:click={toggle} type="button">
    <span class="group-title">{titleMod}</span>
    <span class="group-chevron" class:open={isOpen}>▼</span>
  </button>
  {#if isOpen}
    <div class="group-content">
      <slot />
      {#if settings[enabledKey]}
        <slot name="enabled" />
      {/if}
    </div>
  {/if}
</div>

<style lang="scss">
  .group {
    border-bottom: 1px solid #e0e0e0;
  }

  .group-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    width: 100%;
    padding: 1rem;
    background: none;
    border: none;
    cursor: pointer;
    font-size: 1rem;
    font-weight: 600;
    color: #161616;
    text-align: left;

    &:hover {
      background-color: #f4f4f4;
    }
  }

  .group-title {
    flex: 1;
  }

  .group-chevron {
    transition: transform 0.2s ease;
    font-size: 0.75rem;
    color: #161616;

    &.open {
      transform: rotate(180deg);
    }
  }

  .group-content {
    padding: 0 1rem 1rem 1rem;
  }
</style>
