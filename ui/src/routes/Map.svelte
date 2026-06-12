{#if import.meta.env.VITE_ENABLE_MAP === 'true'}
    {#await import('../map/Map.svelte') then { default: MapView }}
        <MapView />
    {/await}
{/if}
