<script lang="ts">
  import { getContext, createEventDispatcher } from "svelte";
  import { pointer } from "d3-selection";
  import type { Point } from "./model";

  const dispatch = createEventDispatcher();

  export let value: Point;
  export let mapItemId: string = ""
  export let editItemId: null | string = null;
  export let stroke = "yellow";
  export let strokeChoose = "black";
  export let strokeActive = "black";
  export let strokePassive = "grey";
  export let fill = "transparent";
  export let fillActive = "transparent";
  export let fillPassive = "transparent";
  export let strokeWidth = 0.035;
  export let r = 0.18;

  interface DragContext {
    svg: SVGSVGElement;
    contentGroup: SVGGElement;
  }

  const dragContext = getContext<DragContext>("map-drag");

  let hovered = false;

  $: stroke =
    mapItemId === editItemId
      ? strokeActive
      : hovered
        ? "blue"
        : editItemId === null
          ? strokeChoose
          : strokePassive;
  $: fill = editItemId === null ? fillActive : mapItemId === editItemId ? fillActive : fillPassive;

  function click() {
    editItemId = mapItemId
  }

  let isDragging = false;

  function onMouseDown(event: MouseEvent) {
    if (!dragContext) {
      return;
    }
    event.stopPropagation();
    editItemId = mapItemId;
    isDragging = true;

    const handleMouseMove = (e: MouseEvent) => {
      if (!isDragging || !dragContext) return;
      const [x, y] = pointer(e, dragContext.contentGroup);
      dispatch("move", { x, y });
    };

    const handleMouseUp = () => {
      isDragging = false;
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
    };

    window.addEventListener("mousemove", handleMouseMove);
    window.addEventListener("mouseup", handleMouseUp);
  }
</script>

<circle
  cx={value.x}
  cy={value.y}
  {r}
  {stroke}
  {fill}
  stroke-width={strokeWidth}
  role="none"
  on:click={click}
  on:mousedown={onMouseDown}
  on:mouseenter={() => (hovered = true)}
  on:mouseleave={() => (hovered = false)}
  style="cursor: move;"
>
  <title>test</title>
</circle>
