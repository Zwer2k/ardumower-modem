<script lang="ts">
  import { Button, ComboBox, Dropdown, Row } from "carbon-components-svelte";
  import IconPen from "carbon-icons-svelte/lib/Pen.svelte";
  import IconSplit from "carbon-icons-svelte/lib/Split.svelte";
  import IconAdd from "carbon-icons-svelte/lib/Add.svelte";
  import IconTrashCan from "carbon-icons-svelte/lib/TrashCan.svelte";
  import type { EditItem } from "../interactions/map-edit";
  import type { MapArea } from "../model";

  export let edit: boolean;
  export let editCategory: MapArea = "perimeter";
  export let categoryItems: { id: string; text: string }[] = [];
  export let editItems: EditItem[];
  export let selectedId: string | null;
  export let editPoint: boolean;
  export let editEdge: boolean;
  export let drawActive: boolean;
  export let mowerPos: { x: number; y: number } | null;
  export let nearPos: boolean;
  export let onSelectCategory: (e: CustomEvent) => void;
  export let onSelect: (e: CustomEvent) => void;
  export let onClear: () => void;
  export let onDrawClick: () => void;
  export let onSplitClick: () => void;
  export let onAddClick: () => void;
  export let onDeleteClick: () => void;
  export let shouldFilterItem: (item: EditItem, value: string) => boolean;

  function handleSelect(e: CustomEvent) {
    onSelect(e);
  }

  function handleCategorySelect(e: CustomEvent) {
    onSelectCategory(e);
  }
</script>

<Row class="map-edit-row">
  <div class="edit-category">
    <Dropdown
      disabled={!edit || drawActive}
      labelText="Ebene bearbeiten"
      selectedId={editCategory}
      items={categoryItems}
      on:select={handleCategorySelect}
    />
  </div>
  <!-- Hidden legacy point/edge selector, retained for future use -->
  <div class="edit-combo" hidden>
    <ComboBox
      disabled={!edit || drawActive}
      placeholder="Select item to edit"
      items={editItems}
      selectedId={selectedId}
      on:select={handleSelect}
      on:clear={onClear}
      {shouldFilterItem}
    />
  </div>
  <div class="edit-actions">
    <div class="action-btns">
      <Button
        kind={drawActive ? "primary" : "tertiary"}
        size="small"
        disabled={!drawActive && !editEdge}
        icon={IconPen}
        iconDescription="Draw"
        on:click={onDrawClick}
      />
      <Button
        kind="tertiary"
        size="small"
        disabled={drawActive || !editEdge}
        icon={IconSplit}
        iconDescription="Split"
        on:click={onSplitClick}
      />
      <Button
        kind="tertiary"
        size="small"
        disabled={drawActive || !mowerPos || nearPos}
        icon={IconAdd}
        iconDescription="Add"
        on:click={onAddClick}
      >
        <span class="btn-label">Add</span>
      </Button>
      <Button
        kind="danger"
        size="small"
        disabled={drawActive || !edit || !editPoint}
        on:click={onDeleteClick}
        icon={IconTrashCan}
        iconDescription="Delete"
      />
    </div>
  </div>
</Row>

<style>
  .action-btns {
    display: flex;
    flex-wrap: nowrap;
  }
  .edit-category {
    flex: 1 1 auto;
    min-width: 0;
  }
  .edit-category :global(.bx--dropdown) {
    width: 100%;
    min-width: 0;
  }
  .edit-combo {
    flex: 1 1 auto;
    min-width: 0;
  }
  .edit-combo :global(.bx--combo-box) {
    width: 100%;
    min-width: 0;
  }
  .edit-actions {
    flex: 0 0 auto;
  }
  .btn-label {
    margin-left: 0.25rem;
  }
  .action-btns :global(.bx--btn) {
    padding-left: 0.3125rem !important;
    padding-right: 0.3125rem !important;
  }
  .action-btns :global(.bx--btn .bx--btn__icon) {
    position: static;
    margin-left: 0.5rem;
    margin-right: 0.25rem;
  }
  @media (max-width: 800px) {
    .btn-label {
      display: none;
    }
    .action-btns :global(.bx--btn) {
      width: 2rem;
      padding: 0 !important;
      justify-content: center;
    }
    .action-btns :global(.bx--btn .bx--btn__icon) {
      position: static;
      margin: 0;
    }
  }
</style>
