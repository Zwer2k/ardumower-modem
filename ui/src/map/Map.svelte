<script lang="ts">
  import { TextInput, Dropdown, Row, Grid } from "carbon-components-svelte";
  import { onMount } from "svelte";
  import Canvas from "./Canvas.svelte";
  import Exclusion from "./Exclusion.svelte";
  import Track from "./Track.svelte";
  import Perimeter from "./Perimeter.svelte";
  import Waypoints from "./Waypoints.svelte";
  import Dockpoints from "./Dockpoints.svelte";
  import MowerPosition from "./MowerPosition.svelte";
  import { MapStore, cloneMap } from "./service";
  import { socketStore, socketService } from "../stores/socket";
  import { openConfirm } from "../stores/confirm-dialog";
  import { mapWorkflowStore, isMapDirty } from "./map-workflow";
import { setMapDirty } from "./services/map-sync";
  import { get } from "svelte/store";
  import MowSettingsDialog from "./MowSettingsDialog.svelte";
  import MapToolbar from "./toolbar/MapToolbar.svelte";
  import MapManagementToolbar from "./toolbar/MapManagementToolbar.svelte";
  import MapEditToolbar from "./toolbar/MapEditToolbar.svelte";
  import MapCalculateToolbar from "./toolbar/MapCalculateToolbar.svelte";
  import MapStatusOverlay from "./overlay/MapStatusOverlay.svelte";
  import MapGotoOverlay from "./overlay/MapGotoOverlay.svelte";
  import {
    buildEditItems,
    deletePointByEditItemId,
    splitEdgeByEditItemId,
    addPointAtMowerPosition,
    startDrawMode,
    moveFloatingPoint,
    placeFloatingPoint,
    findCrossedCandidate,
    switchToCandidate,
    findPointIndex,
    removeFloatingPoint,
    buildPointId,
    buildInitialCandidates,
    type EditItem,
  } from "./interactions/map-edit";
  import { createGotoState } from "./interactions/map-goto";
  import { createCompassState } from "./interactions/map-compass";
  import type { Point } from "./model";

  let edit = false;
  let wasEditing = false;
  let showMowSettings = false;
  let selectedId: string | null = null;
  let editItemId: string | null = null;
  let editItems: EditItem[] = [];

  let editPoint = false;
  let editEdge = false;

  $: editPoint = edit && !!editItemId && editItemId.indexOf("-point-") !== -1;
  $: editEdge = edit && !!editItemId && editItemId.indexOf("-edge-") !== -1;
  $: selectedId = editItemId;

  // ─── Map management ────────────────────────────────────────────────────────
  let showManage = false;
  let showCalculate = false;
  let selectedMapId: string = "";
  let dropdownSelectedId: string = "";

  function closePanels() {
    showManage = false;
    edit = false;
    showCalculate = false;
    stopDraw();
  }

  function toggleManage() {
    if (showManage) {
      closePanels();
    } else {
      showManage = true;
      edit = false;
      showCalculate = false;
      stopDraw();
    }
  }

  function toggleEdit() {
    if (edit) {
      closePanels();
    } else {
      stopDraw();
      edit = true;
      showManage = false;
      showCalculate = false;
    }
  }

  function toggleCalculate() {
    if (showCalculate) {
      closePanels();
    } else {
      showCalculate = true;
      showManage = false;
      edit = false;
      stopDraw();
    }
  }

  $: mapOptions = (() => {
    const seen = new Set<string>();
    const opts: { id: string; text: string }[] = [];
    const isNew = $socketStore.isNewMap || $mapWorkflowStore.state === "creating";
    if (isNew) {
      const name = $mapWorkflowStore.pendingName || `Karte ${$socketStore.maps.length + 1}`;
      opts.push({
        id: "__unsaved__",
        text: `${name} (unsaved)`,
      });
    }
    for (const m of $socketStore.maps) {
      if (seen.has(m.id)) continue;
      seen.add(m.id);
      opts.push({
        id: m.id,
        text: `${m.name} (${m.area.toFixed(1)} m²)${m.id === $socketStore.activeMapId ? ' ★ default' : ''}`,
      });
    }
    return opts;
  })();

  $: {
    const isNew = $socketStore.isNewMap || $mapWorkflowStore.state === "creating";
    const baseId = isNew ? "__unsaved__" : ($socketStore.currentMapId || "");
    if (!$mapWorkflowStore.renameMode && $mapWorkflowStore.state !== "loading") {
      selectedMapId = baseId;
      dropdownSelectedId = baseId;
    }
  }

  $: effectiveMapId = $socketStore.currentMapId || "";
  $: effectiveMap = $socketStore.maps.find((m) => m.id === effectiveMapId);
  $: effectiveMapName = effectiveMap?.name || "";
  $: isDirty = $isMapDirty;
  $: selectedIsCurrentMap = selectedMapId === (effectiveMapId || "__unsaved__");
  $: canSave = isDirty && selectedIsCurrentMap && !$mapWorkflowStore.renameMode && !workflowBusy;
  $: canRevert = isDirty && selectedIsCurrentMap && !$mapWorkflowStore.renameMode && !workflowBusy;
  $: canRename = ($MapStore.map?.perimeter.points.length ?? 0) >= 3 && !!effectiveMapId;
  $: workflowBusy = $mapWorkflowStore.state === "loading" || $mapWorkflowStore.state === "saving" || $mapWorkflowStore.state === "renaming" || $mapWorkflowStore.state === "deleting";

  onMount(() => {
    socketService.sendListMaps();
  });

  async function onSelectMap(e: CustomEvent) {
    const id = e.detail?.selectedId;
    if (!id) return;
    if (id === "__unsaved__") {
      selectedMapId = id;
      dropdownSelectedId = id;
      return;
    }
    if ($mapWorkflowStore.state === "loading" && id === $mapWorkflowStore.pendingLoadId) return;

    const dirty = get(isMapDirty);
    if (dirty && id !== effectiveMapId && !!effectiveMapId) {
      const choice = await openConfirm({
        title: "Ungespeicherte Änderungen",
        message: "Änderungen speichern, bevor zu einer anderen Karte gewechselt wird?",
        confirmText: "Speichern",
        cancelText: "Verwerfen",
      });
      if (choice) {
        onSaveMap();
      } else {
        mapWorkflowStore.resetDirtyState();
      }
    }
    selectedMapId = id;
    dropdownSelectedId = id;
    stopDraw();
    mapWorkflowStore.startLoadMap(id);
    socketService.sendLoadMap(id);
  }

  async function onNewMap() {
    if (isDirty && selectedIsCurrentMap) {
      const choice = await openConfirm({
        title: "Ungespeicherte Änderungen",
        message: "Änderungen speichern, bevor eine neue Karte erstellt wird?",
        confirmText: "Speichern",
        cancelText: "Verwerfen",
      });
      if (choice) {
        onSaveMap();
      } else {
        mapWorkflowStore.resetDirtyState();
      }
    }
    stopDraw();
    const defaultName = `Karte ${$socketStore.maps.length + 1}`;
    compassState.reset();
    socketService.sendDiscardMap();
    mapWorkflowStore.startNewMap(defaultName);
    showManage = true;
  }

  $: if (
    ($socketStore.currentMapMeta && !$socketStore.currentMapId) &&
    !$mapWorkflowStore.renameMode &&
    $mapWorkflowStore.state !== "loading" &&
    $mapWorkflowStore.state !== "saving" &&
    $mapWorkflowStore.state !== "deleting" &&
    !$socketStore.isLoadingMap &&
    $mapWorkflowStore.pendingName === ""
  ) {
    const defaultName = `Karte ${$socketStore.maps.length + 1}`;
    mapWorkflowStore.startNewMap(defaultName);
    showManage = true;
  }

  function onSaveMap() {
    const name = $mapWorkflowStore.pendingName || effectiveMapName || `Karte ${$socketStore.maps.length + 1}`;
    mapWorkflowStore.startSaveMap(name, compassState.rotation);
    socketService.sendSaveMap(name, compassState.rotation);
  }

  async function onDiscardMap() {
    const target = selectedMapId || dropdownSelectedId || effectiveMapId;
    if (!target) return;
    const message = target === "__unsaved__" || $socketStore.isNewMap
      ? "Neue Karte verwerfen? Ungespeicherte Änderungen gehen verloren."
      : "Änderungen an der Karte verwerfen und gespeicherte Version laden?";
    const choice = await openConfirm({
      title: "Änderungen verwerfen",
      message,
      confirmText: "Verwerfen",
      cancelText: "Abbrechen",
      kind: "danger",
    });
    if (!choice) return;
    stopDraw();
    if (target === "__unsaved__" || $socketStore.isNewMap) {
      mapWorkflowStore.resetDirtyState();
      socketService.sendDiscardMap();
    } else {
      mapWorkflowStore.resetDirtyState();
      mapWorkflowStore.startLoadMap(target);
      socketService.sendLoadMap(target);
    }
  }

  function startRename() {
    mapWorkflowStore.startRename(effectiveMapName);
  }

  function confirmRename() {
    const result = mapWorkflowStore.confirmRename($socketStore.currentMapId, effectiveMapName);
    if (result.action === "save") {
      socketService.sendSaveMap(result.name, compassState.rotation);
    } else if (result.action === "rename") {
      socketService.sendRenameMap($socketStore.currentMapId, result.name);
    }
  }

  function cancelRename() {
    mapWorkflowStore.cancelRename(effectiveMapName);
  }

  async function onDeleteMap() {
    const target = dropdownSelectedId || effectiveMapId;
    if (!target || target === "__unsaved__") return;
    const choice = await openConfirm({
      title: "Karte löschen",
      message: "Karte wirklich löschen? Dies kann nicht rückgängig gemacht werden.",
      confirmText: "Löschen",
      cancelText: "Abbrechen",
      kind: "danger",
    });
    if (!choice) return;
    mapWorkflowStore.startDeleteMap(target);
    socketStore.update((s) => {
      const nextMaps = s.maps.filter((m) => m.id !== target);
      const nextCurrentMapId = s.currentMapId === target ? "" : s.currentMapId;
      const nextActiveMapId = s.activeMapId === target ? "" : s.activeMapId;
      return {
        ...s,
        maps: nextMaps,
        currentMapId: nextCurrentMapId,
        currentMapMeta: nextCurrentMapId ? s.currentMapMeta : null,
        activeMapId: nextActiveMapId,
        isNewMap: nextCurrentMapId ? s.isNewMap : false,
      };
    });
    if (dropdownSelectedId === target) {
      const fallbackId = effectiveMapId !== target ? effectiveMapId : "";
      selectedMapId = fallbackId;
      dropdownSelectedId = fallbackId;
    }
    socketService.sendDeleteMap(target);
  }

  function onSetDefaultMap() {
    const target = dropdownSelectedId || effectiveMapId;
    if (!target || target === "__unsaved__") return;
    socketService.sendSetActiveMap(target);
  }

  // ─── Point counts & sync state ─────────────────────────────────────────────
  $: busy = !!$socketStore.state?.progressOp;
  $: perimeterPoints = $MapStore.map?.perimeter.points.length ?? 0;
  $: exclusionPoints = $MapStore.map?.exclusions.map((e) => e.points.length) ?? [];
  $: dockpointsPoints = $MapStore.map?.dockpoints.points.length ?? 0;
  $: waypointsPoints = $MapStore.map?.waypoints.points.length ?? 0;
  $: totalPoints = perimeterPoints + dockpointsPoints + waypointsPoints + exclusionPoints.reduce((a, b) => a + b, 0);

  $: nearPos = !!(mowerPos && mowerPos.x !== 0 && mowerPos.y !== 0
    && $MapStore.map?.perimeter.points.some((pt) => {
        const dx = pt.x - mowerPos.x;
        const dy = pt.y + mowerPos.y;
        return dx * dx + dy * dy < 0.0025;
      }));

  // ─── Edit/draw state ───────────────────────────────────────────────────────
  let drawActive = false;
  let drawArea: 'perimeter' | 'exclusion' | 'dockpoints' | 'waypoints' | null = null;
  let drawExclusionIndex: number | undefined = undefined;
  let drawCandidates: Array<{
    area: 'perimeter' | 'exclusion' | 'dockpoints' | 'waypoints';
    exclusionIndex?: number;
    begin: Point;
    end: Point;
  }> = [];
  let floatingPoint: Point | null = null;

  $: editItems = $MapStore && $MapStore.map ? buildEditItems($MapStore.map) : [];

  function selectEditItem(e: CustomEvent) {
    editItemId = e.detail?.selectedId ?? null;
  }

  function shouldFilterItem(item: { text: string }, value: string) {
    return item.text.toLowerCase().includes(value.toLowerCase());
  }

  function clearEditItem() {
    editItemId = null;
  }

  function onDeleteClick() {
    if (!editItemId) return;
    deletePointByEditItemId(editItemId);
  }

  function selectNewPoint(id: string) {
    editItemId = id;
  }

  function onSplitClick() {
    if (!editItemId) return;
    const newPointId = splitEdgeByEditItemId(editItemId);
    if (newPointId) {
      selectNewPoint(newPointId);
    }
  }

  function onAddClick() {
    if (!mowerPos) return;
    const index = addPointAtMowerPosition(mowerPos, editItemId, editEdge);
    if (index !== null) {
      const newPointId = buildPointId(editItemId, index);
      if (newPointId) selectNewPoint(newPointId);
    }
  }

  function onDrawClick() {
    if (drawActive) {
      stopDraw();
      return;
    }
    if (!editItemId) return;
    const result = startDrawMode(editItemId);
    if (!result) return;
    drawActive = true;
    drawArea = result.area;
    drawExclusionIndex = result.exclusionIndex;
    floatingPoint = result.midPoint;
    drawCandidates = buildInitialCandidates(
      result.area,
      result.exclusionIndex,
      result.begin,
      result.end,
      result.midPoint
    );
    selectNewPoint(result.newPointId);
  }

  function stopDraw() {
    if (!drawActive) return;
    if (floatingPoint && drawArea) {
      removeFloatingPoint(drawArea, drawExclusionIndex, floatingPoint);
    }
    drawActive = false;
    drawArea = null;
    drawExclusionIndex = undefined;
    drawCandidates = [];
    floatingPoint = null;
  }

  function onKeyDown(event: KeyboardEvent) {
    if (event.key === "Escape" && drawActive) {
      stopDraw();
    }
  }

  onMount(() => {
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  });

  function onDrawMapClick(event: CustomEvent<{ x: number; y: number }>) {
    if (!drawActive || !floatingPoint || drawCandidates.length === 0) return;
    const { x, y } = event.detail;

    const crossed = findCrossedCandidate(
      x,
      y,
      drawArea,
      drawExclusionIndex,
      floatingPoint,
      drawCandidates
    );
    if (crossed) {
      const switched = switchToCandidate(
        crossed,
        floatingPoint,
        drawArea,
        drawExclusionIndex,
        drawCandidates
      );
      if (switched) {
        drawArea = switched.drawArea;
        drawExclusionIndex = switched.drawExclusionIndex;
        drawCandidates = switched.drawCandidates;
        floatingPoint = switched.floatingPoint;
      }
    }

    const placeResult = placeFloatingPoint(
      x,
      y,
      drawArea,
      drawExclusionIndex,
      floatingPoint,
      drawCandidates
    );
    if (placeResult) {
      drawArea = placeResult.drawArea;
      drawExclusionIndex = placeResult.drawExclusionIndex;
      drawCandidates = placeResult.drawCandidates;
      floatingPoint = placeResult.floatingPoint;
      const idx = findPointIndex(drawArea, drawExclusionIndex, floatingPoint);
      const newPointId = buildPointId(editItemId, idx);
      if (newPointId) selectNewPoint(newPointId);
    }
  }

  function onMouseMove(event: CustomEvent<{ x: number; y: number }>) {
    if (!drawActive || !floatingPoint) return;
    const { x, y } = event.detail;

    const crossed = findCrossedCandidate(
      x,
      y,
      drawArea,
      drawExclusionIndex,
      floatingPoint,
      drawCandidates
    );
    if (crossed) {
      const switched = switchToCandidate(
        crossed,
        floatingPoint,
        drawArea,
        drawExclusionIndex,
        drawCandidates
      );
      if (switched) {
        drawArea = switched.drawArea;
        drawExclusionIndex = switched.drawExclusionIndex;
        drawCandidates = switched.drawCandidates;
        floatingPoint = switched.floatingPoint;
      }
    }

    const moveResult = moveFloatingPoint(
      x,
      y,
      drawArea,
      drawExclusionIndex,
      floatingPoint,
      drawCandidates
    );
    if (moveResult) {
      floatingPoint = moveResult.newFloatingPoint;
      drawCandidates = moveResult.newCandidates;
    }
  }

  // ─── Sync-Status: CRC aus Metadaten ────────────────────────────────────────
  $: hasState = $socketStore.state !== null;
  $: storedCrc = $socketStore.currentMapMeta?.crc ?? 0;
  $: sync = { needsUpload: hasState && ($socketStore.state?.map_crc ?? 0) !== storedCrc };

  $: if (!edit && wasEditing && $MapStore && $MapStore.map) {
    wasEditing = false;
    const m = $MapStore.map;
    socketService.sendMap({
      perimeter: m.perimeter.points,
      exclusions: m.exclusions.map((e) => e.points),
      dockpoints: m.dockpoints.points,
      waypoints: m.waypoints.points,
      rotation: compassState.rotation,
    });
  } else if (edit) {
    wasEditing = true;
  }

  // ─── Goto state ────────────────────────────────────────────────────────────
  const goto = createGotoState();
  $: mowerPos = $socketStore.state?.position ?? null;
  $: hasMap = $MapStore?.map?.perimeter?.points?.length > 0;
  $: targetSet = goto.targetSet;
  $: targetPos = goto.targetPos;
  $: isDriving = goto.isDriving;
  $: targetDist = goto.targetDist;
  $: targetBearing = goto.targetBearing;

  function onMapClick(event: CustomEvent<{ x: number; y: number }>) {
    if (drawActive) {
      onDrawMapClick(event);
      return;
    }
    if (edit) return;
    if (!hasMap) return;
    if (showManage || showCalculate) return;
    if (sync.needsUpload) return;
    const { x, y } = event.detail;
    goto.setTarget(x, y);
  }

  function startDrive() {
    if (!goto.targetSet || !goto.targetPos) return;
    socketService.sendNavigateTo(goto.targetPos.x, -goto.targetPos.y);
    goto.startDrive();
  }

  function stopDrive() {
    socketService.sendJoystickMove(0, 0);
    goto.stopDrive();
  }

  function clearTarget() {
    socketService.sendJoystickMove(0, 0);
    goto.clearTarget();
  }

  // ─── Compass state ─────────────────────────────────────────────────────────
  const compassState = createCompassState();
</script>

<div class="map-dashboard">
  <div class="map-toolbar-wrap">
    <Grid class="map-toolbar">
      <Row class="map-toolbar-main">
        <div class="toolbar-main-group">
          <div class="toolbar-dropdown">
            {#if $mapWorkflowStore.renameMode}
              <TextInput
                placeholder="Map name"
                bind:value={$mapWorkflowStore.pendingName}
              />
            {:else}
              <Dropdown
                placeholder="Select map"
                items={mapOptions}
                selectedId={selectedMapId}
                on:select={onSelectMap}
              />
            {/if}
          </div>
          <MapToolbar
            {workflowBusy}
            {busy}
            renameMode={$mapWorkflowStore.renameMode}
            {showManage}
            {edit}
            {showCalculate}
            canConfirmRename={!!$mapWorkflowStore.pendingName && $mapWorkflowStore.pendingName !== effectiveMapName}
            onUpload={() => socketService.sendUploadMap()}
            onToggleManage={toggleManage}
            onToggleEdit={toggleEdit}
            onToggleCalculate={toggleCalculate}
            onConfirmRename={confirmRename}
            onCancelRename={cancelRename}
          />
        </div>
        {#if !showManage && !edit && !showCalculate && !sync.needsUpload}
          <div class="toolbar-goto-group">
            {#if hasMap}
              {#if targetSet}
                <span class="goto-badge">{targetDist.toFixed(1)}m / {targetBearing.toFixed(0)}°</span>
                {#if isDriving}
                  <button class="goto-btn stop" on:click={stopDrive}>Stop</button>
                {:else}
                  <button class="goto-btn drive" on:click={startDrive}>Drive</button>
                {/if}
                <button class="goto-btn clear" on:click={clearTarget}>✕</button>
              {:else}
                <span class="goto-hint">Click map to set target</span>
              {/if}
            {:else}
              <span class="goto-hint">No map loaded</span>
            {/if}
          </div>
        {/if}
      </Row>

      {#if showManage}
        <MapManagementToolbar
          workflow={$mapWorkflowStore}
          {canSave}
          {canRevert}
          {canRename}
          {effectiveMapId}
          pendingName={$mapWorkflowStore.pendingName}
          {effectiveMapName}
          {workflowBusy}
          onSaveMap={onSaveMap}
          onDiscardMap={onDiscardMap}
          startRename={startRename}
          onDeleteMap={onDeleteMap}
          onSetDefaultMap={onSetDefaultMap}
          onNewMap={onNewMap}
        />
      {/if}

      {#if edit}
        <MapEditToolbar
          {edit}
          {editItems}
          bind:selectedId
          {editPoint}
          {editEdge}
          {drawActive}
          {mowerPos}
          {nearPos}
          onSelect={selectEditItem}
          onClear={clearEditItem}
          onDrawClick={onDrawClick}
          onSplitClick={onSplitClick}
          onAddClick={onAddClick}
          onDeleteClick={onDeleteClick}
          {shouldFilterItem}
        />
      {/if}

      {#if showCalculate}
        <MapCalculateToolbar
          {busy}
          onOpenMowSettings={() => (showMowSettings = true)}
        />
      {/if}
    </Grid>
  </div>
  <div class="map-canvas-wrapper">
    <MapStatusOverlay
      compassRotation={compassState.rotation}
      socketState={$socketStore}
      {perimeterPoints}
      {exclusionPoints}
      {dockpointsPoints}
      {waypointsPoints}
      {totalPoints}
      needsUpload={sync.needsUpload}
      onCompassDown={compassState.onDown}
    />
    <Canvas
      compassRotation={compassState.rotation}
      on:mapclick={onMapClick}
      on:mousemove={onMouseMove}
    >
      {#if $MapStore && $MapStore.map}
        <Track />
        <Perimeter
          value={$MapStore.map.perimeter}
          perimiterId="map-0-perimeter"
          {edit}
          {drawActive}
          bind:editItemId
          onMove={(points) => {
            MapStore.update((s) => ({
              ...s,
              map: {
                ...cloneMap(s.map),
                perimeter: { points: points.map((p) => ({ ...p })) },
              },
            }));
            setMapDirty(true);
          }}
        />
        {#each $MapStore.map.exclusions as exclusion, index}
          <Exclusion
            value={exclusion}
            exclusionId={"map-0-exclusion-" + index}
            {edit}
            {drawActive}
            bind:editItemId
            onMove={(points) => {
              MapStore.update((s) => {
                const updatedExclusions = s.map.exclusions.map((e, i) =>
                  i === index ? { points: points.map((p) => ({ ...p })) } : e
                );
                return {
                  ...s,
                  map: { ...cloneMap(s.map), exclusions: updatedExclusions },
                };
              });
              setMapDirty(true);
            }}
          />
        {/each}
        <Waypoints
          value={$MapStore.map.waypoints}
          waypointsId="map-0-waypoints"
          {edit}
          {drawActive}
          bind:editItemId
          onMove={(points) => {
            MapStore.update((s) => ({
              ...s,
              map: {
                ...cloneMap(s.map),
                waypoints: { points: points.map((p) => ({ ...p })) },
              },
            }));
            setMapDirty(true);
          }}
        />
        <Dockpoints
          value={$MapStore.map.dockpoints}
          dockpointsId="map-0-dockpoints"
          {edit}
          {drawActive}
          bind:editItemId
          onMove={(points) => {
            MapStore.update((s) => ({
              ...s,
              map: {
                ...cloneMap(s.map),
                dockpoints: { points: points.map((p) => ({ ...p })) },
              },
            }));
            setMapDirty(true);
          }}
        />
        <MowerPosition position={mowerPos} />

        {#if drawActive && floatingPoint}
          {#each drawCandidates.filter((c) => c.begin === floatingPoint || c.end === floatingPoint) as c}
            {@const other = c.begin === floatingPoint ? c.end : c.begin}
            <line
              x1={floatingPoint.x}
              y1={floatingPoint.y}
              x2={other.x}
              y2={other.y}
              stroke="#e65100"
              stroke-width="0.04"
              stroke-dasharray="0.08, 0.08"
              opacity="0.7"
              pointer-events="none"
            />
          {/each}
        {/if}

        <MapGotoOverlay
          {targetPos}
          {mowerPos}
          showTarget={targetSet && !showManage && !edit && !showCalculate && !sync.needsUpload}
          showLine={targetSet && !!mowerPos && (mowerPos.x !== 0 || mowerPos.y !== 0) && !showManage && !edit && !showCalculate && !sync.needsUpload}
        />
      {/if}
    </Canvas>
    {#if targetSet && !showManage && !edit && !showCalculate && !sync.needsUpload}
      <div class="goto-floater">
        <span class="goto-badge">{targetDist.toFixed(1)}m / {targetBearing.toFixed(0)}°</span>
        {#if isDriving}
          <button class="goto-btn stop" on:click={stopDrive}>Stop</button>
        {:else}
          <button class="goto-btn drive" on:click={startDrive}>Drive</button>
        {/if}
        <button class="goto-btn clear" on:click={clearTarget}>✕</button>
      </div>
    {/if}
  </div>
</div>

<MowSettingsDialog bind:open={showMowSettings} />

<style>
  .map-dashboard {
    position: relative;
    width: 100%;
    height: 100%;
    padding-top: 48px;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-sizing: border-box;
  }
  .map-toolbar-wrap {
    position: relative;
    flex-shrink: 0;
    container-type: inline-size;
    width: 100%;
    max-width: 1000px;
    margin: 0 auto;
  }
  .map-canvas-wrapper {
    position: relative;
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }
  .goto-floater {
    position: absolute;
    bottom: 1rem;
    left: 50%;
    transform: translateX(-50%);
    display: flex;
    align-items: center;
    gap: 0.25rem;
    background: rgba(255, 255, 255, 0.9);
    padding: 0.5rem;
    border-radius: 4px;
    border: 1px solid #e0e0e0;
    z-index: 10;
  }
  .goto-badge {
    font-size: 0.75em;
    padding: 3px 8px;
    background: #fff3e0;
    border: 1px solid #ffab00;
    border-radius: 4px;
    color: #b06000;
    font-family: monospace;
    margin-right: 4px;
  }
  .goto-btn {
    padding: 4px 10px;
    border: 1px solid #ccc;
    border-radius: 4px;
    font-size: 0.75em;
    cursor: pointer;
    background: #f4f4f4;
  }
  .goto-btn.drive {
    background: #e8f5e9;
    border-color: #4caf50;
    color: #2e7d32;
  }
  .goto-btn.stop {
    background: #ffebee;
    border-color: #ef5350;
    color: #c62828;
  }
  .goto-btn.clear {
    padding: 4px 8px;
  }
  .goto-hint {
    font-size: 0.75em;
    color: #888;
    font-style: italic;
    padding-left: 0.5rem;
  }
  :global(.map-toolbar .bx--row) {
    flex-wrap: wrap;
    align-items: center;
  }
  :global(.map-toolbar .bx--list-box) {
    height: 32px;
    max-height: 32px;
  }
  :global(.map-toolbar .bx--list-box__field) {
    height: 32px;
  }
  :global(.map-toolbar .bx--list-box__menu) {
    top: 32px;
  }
  .toolbar-main-group {
    display: flex;
    flex: 1 1 auto;
    align-items: center;
    gap: 0.25rem;
    min-width: 0;
  }
  .toolbar-dropdown {
    flex: 1 1 auto;
    min-width: 8rem;
  }
  .toolbar-dropdown :global(.bx--dropdown),
  .toolbar-dropdown :global(.bx--text-input) {
    width: 100%;
  }
  .toolbar-goto-group {
    display: flex;
    flex: 0 0 auto;
    align-items: center;
    justify-content: flex-end;
    gap: 0.25rem;
    margin-left: auto;
    padding-left: 0.75rem;
  }
  @container (max-width: 800px) {
    .toolbar-main-group {
      width: 100%;
    }
  }
</style>
