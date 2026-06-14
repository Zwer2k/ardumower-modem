<script lang="ts">
  import { onMount, setContext } from "svelte";
  import { zoom } from "d3-zoom";
  import { select, pointer } from "d3-selection";

  import type { MapPresentation } from "./model";
  import { MapStore } from "./service";
  import { createEventDispatcher } from "svelte";

  const dispatch = createEventDispatcher();

  function transform(p: MapPresentation): string {
    return `rotate(${p.rotation})`;
  }

  let svg: SVGSVGElement;
  let g: SVGGElement;
  let contentGroup: SVGGElement;

  setContext("map-drag", {
    get svg() { return svg; },
    get contentGroup() { return contentGroup; }
  });

  onMount(() => {
    if (svg && g) {
      select(svg).call(
        zoom().on("zoom", ({ transform }) => {
          const { k, x, y } = transform;
          select(g).attr("transform", `translate(${x}, ${y}) scale(${k})`);
        })
      );
    }
  });

  function handleClick(event: MouseEvent) {
    if (!contentGroup || !svg) return;
    const [x, y] = pointer(event, contentGroup);
    dispatch('mapclick', { x, y });
  }

  function handleMouseMove(event: MouseEvent) {
    if (!contentGroup || !svg) return;
    const [x, y] = pointer(event, contentGroup);
    dispatch('mousemove', { x, y });
  }
</script>

<main>
  <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions -->
  <svg
    bind:this={svg}
    viewBox={$MapStore.presentation.viewBox}
    preserveAspectRatio="xMidYMid meet"
    width="100%"
    height="100%"
    onclick={handleClick}
    onmousemove={handleMouseMove}
  >
    <defs>
      <marker id="arrow-perimeter" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="red" />
      </marker>
      <marker id="arrow-exclusion" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="black" />
      </marker>
      <marker id="arrow-waypoints" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="blue" />
      </marker>
      <marker id="arrow-dockpoints" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="orange" />
      </marker>
    </defs>
    <g bind:this={g}>
      <g bind:this={contentGroup} transform={transform($MapStore.presentation)}>
        <slot />
      </g>
    </g>
  </svg>
</main>

<style>
  main {
    width: 100%;
    height: 100%;
    display: block;
  }
  svg {
    width: 100%;
    height: 100%;
    display: block;
  }
</style>
