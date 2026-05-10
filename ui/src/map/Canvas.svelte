<script lang="ts">
  import { onMount } from "svelte";
  import { zoom } from "d3-zoom";
  import { select } from "d3-selection";

  import type { MapPresentation } from "./model";
  import { MapStore } from "./service";

  function transform(p: MapPresentation): string {
    return `rotate(${p.rotation})`;
  }

  let svg: SVGSVGElement;
  let g: SVGGElement;

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
</script>

<main>
  <svg
    bind:this={svg}
    viewBox={$MapStore.presentation.viewBox}
    preserveAspectRatio="xMidYMid meet"
    width="100%"
    height="100%"
  >
    <g bind:this={g}>
      <g transform={transform($MapStore.presentation)}>
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
