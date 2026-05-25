<script lang="ts">
  import { onMount } from "svelte";
  import { zoom } from "d3-zoom";
  import { select } from "d3-selection";

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
    const pt = svg.createSVGPoint();
    pt.x = event.clientX;
    pt.y = event.clientY;
    const ctm = contentGroup.getScreenCTM();
    if (!ctm) return;
    const local = pt.matrixTransform(ctm.inverse());
    dispatch('mapclick', { x: local.x, y: local.y });
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
  >
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
