<script lang="ts">
  import {
    Button,
    ButtonSet,
    ComboBox,
    Dropdown,
    TextInput,
    Slider,
    Row,
    Column,
    Grid,
  } from "carbon-components-svelte";
  import IconAdd from "carbon-icons-svelte/lib/Add.svelte";
  import IconEdit from "carbon-icons-svelte/lib/Edit.svelte";
  import IconPen from "carbon-icons-svelte/lib/Pen.svelte";
  import IconSplit from "carbon-icons-svelte/lib/Split.svelte";
  import IconCut from "carbon-icons-svelte/lib/Cut.svelte";
  import IconTrashCan from "carbon-icons-svelte/lib/TrashCan.svelte";
  import IconUpload from "carbon-icons-svelte/lib/Upload.svelte";
  import IconSave from "carbon-icons-svelte/lib/Save.svelte";
  import IconStar from "carbon-icons-svelte/lib/Star.svelte";
  import IconNew from "carbon-icons-svelte/lib/DocumentAdd.svelte";
  import IconTools from "carbon-icons-svelte/lib/Tools.svelte";
  import IconCheckmark from "carbon-icons-svelte/lib/Checkmark.svelte";
  import IconClose from "carbon-icons-svelte/lib/Close.svelte";
  import IconSettings from "carbon-icons-svelte/lib/Settings.svelte";
  import IconMagicWand from "carbon-icons-svelte/lib/MagicWand.svelte";
  import { onMount } from "svelte";
  import Canvas from "./Canvas.svelte";
  import Exclusion from "./Exclusion.svelte";
  import Track from "./Track.svelte";
  import { pointsToEdges } from "./geometry";
  import type { Point, Edge } from "./model";
  import Perimeter from "./Perimeter.svelte";
  import Waypoints from "./Waypoints.svelte";
  import Dockpoints from "./Dockpoints.svelte";
  import MowerPosition from "./MowerPosition.svelte";
  import { MapStore, emptyMap, emptyPresentation, currentMapRotationStore } from "./service";
  import { socketStore, socketService } from "../stores/socket";
  import { mapMetaStore } from "../stores/socket";
  import { mapWorkflowStore, isMapDirty } from "./map-workflow";
  import MowSettingsDialog from "./MowSettingsDialog.svelte";

  interface EditItem {
    id: string;
    text: string;
  }

  let edit = false;
  let wasEditing = false;
  let showMowSettings = false;
  let selectedId: string | null = null;
  let editItemId: string | null = null;
  let editItems: EditItem[] = [];

  let editPoint = false;
  let editEdge = false;

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

  // Dropdown-Selektion an Backend-Zustand binden, aber nicht überschreiben,
  // wenn der Benutzer gerade im Rename-Modus ist oder eine Karte geladen wird.
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
  $: canRename = ($MapStore.map?.perimeter.points.length ?? 0) >= 3 && !!effectiveMapId;
  $: workflowBusy = $mapWorkflowStore.state === "loading" || $mapWorkflowStore.state === "saving" || $mapWorkflowStore.state === "renaming" || $mapWorkflowStore.state === "deleting";

  onMount(() => {
    socketService.sendListMaps();
  });

  function onSelectMap(e: CustomEvent) {
    const id = e.detail?.selectedId;
    if (!id) return;
    selectedMapId = id;
    dropdownSelectedId = id;
    if (id === "__unsaved__") return;
    // Wenn für diese ID bereits eine Ladeanfrage läuft, nichts tun (verhindert
    // Doppel-Anfragen, falls das Dropdown ein zweites on:select feuert).
    if ($mapWorkflowStore.state === "loading" && id === $mapWorkflowStore.pendingLoadId) return;
    mapWorkflowStore.startLoadMap(id);
    socketService.sendLoadMap(id);
  }

  function onNewMap() {
    const defaultName = `Karte ${$socketStore.maps.length + 1}`;
    compassRotation = 0;
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
    mapWorkflowStore.startSaveMap(name, compassRotation);
    socketService.sendSaveMap(name, compassRotation);
  }

  function startRename() {
    mapWorkflowStore.startRename(effectiveMapName);
  }

  function confirmRename() {
    const result = mapWorkflowStore.confirmRename($socketStore.currentMapId, effectiveMapName);
    if (result.action === "save") {
      socketService.sendSaveMap(result.name, compassRotation);
    } else if (result.action === "rename") {
      socketService.sendRenameMap($socketStore.currentMapId, result.name);
    }
  }

  function cancelRename() {
    mapWorkflowStore.cancelRename(effectiveMapName);
  }

  function onDeleteMap() {
    const target = dropdownSelectedId || effectiveMapId;
    if (!target || target === "__unsaved__") return;
    if (confirm("Karte wirklich löschen?")) {
      mapWorkflowStore.startDeleteMap(target);
      // Gelöschte Karte sofort aus dem lokalen State entfernen, damit das
      // Dropdown nicht erst auf die Backend-Antwort warten muss.
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
      // Auswahl auf die aktuell geladene Karte oder leer zurücksetzen, falls
      // die gelöschte Karte im Dropdown ausgewählt war.
      if (dropdownSelectedId === target) {
        const fallbackId = effectiveMapId !== target ? effectiveMapId : "";
        selectedMapId = fallbackId;
        dropdownSelectedId = fallbackId;
      }
      socketService.sendDeleteMap(target);
    }
  }

  function onSetDefaultMap() {
    const target = dropdownSelectedId || effectiveMapId;
    if (!target || target === "__unsaved__") return;
    socketService.sendSetActiveMap(target);
  }

  // ─── Point counts per segment ─────────────────────────────────────────────
  $: busy = !!$socketStore.state?.progressOp;
  $: perimeterPoints = $MapStore.map?.perimeter.points.length ?? 0;
  $: exclusionPoints = $MapStore.map?.exclusions.map(e => e.points.length) ?? [];
  $: dockpointsPoints = $MapStore.map?.dockpoints.points.length ?? 0;
  $: waypointsPoints = $MapStore.map?.waypoints.points.length ?? 0;
  $: totalPoints = perimeterPoints + dockpointsPoints + waypointsPoints + exclusionPoints.reduce((a, b) => a + b, 0);

  $: nearPos = !!(mowerPos && mowerPos.x !== 0 && mowerPos.y !== 0
    && $MapStore.map?.perimeter.points.some(pt => {
        const dx = pt.x - mowerPos.x;
        const dy = pt.y + mowerPos.y;
        return dx * dx + dy * dy < 0.0025; // 5cm
      }));

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

  function getPoints(area: string, exclusionIndex?: number): Point[] {
    if (area === 'perimeter') return $MapStore.map.perimeter.points;
    if (area === 'exclusion') return $MapStore.map.exclusions[exclusionIndex!].points;
    if (area === 'dockpoints') return $MapStore.map.dockpoints.points;
    return $MapStore.map.waypoints.points;
  }

  function setPoints(points: Point[], area: string, exclusionIndex?: number) {
    if (area === 'perimeter') {
      $MapStore.map.perimeter.points = points;
      $MapStore.map.perimeter = $MapStore.map.perimeter;
    } else if (area === 'exclusion') {
      $MapStore.map.exclusions[exclusionIndex!].points = points;
      $MapStore.map.exclusions[exclusionIndex!] = $MapStore.map.exclusions[exclusionIndex!];
    } else if (area === 'dockpoints') {
      $MapStore.map.dockpoints.points = points;
      $MapStore.map.dockpoints = $MapStore.map.dockpoints;
    } else {
      $MapStore.map.waypoints.points = points;
      $MapStore.map.waypoints = $MapStore.map.waypoints;
    }
  }

  function removePoint(pts: Point[], pt: Point): Point[] {
    const idx = pts.indexOf(pt);
    if (idx !== -1) {
      const copy = [...pts];
      copy.splice(idx, 1);
      return copy;
    }
    return [...pts];
  }

  function insertBetween(pts: Point[], a: Point, b: Point, pt: Point): Point[] {
    const copy = [...pts];
    const idxA = copy.indexOf(a);
    const idxB = copy.indexOf(b);
    if (idxA === -1 || idxB === -1) return copy;

    if (idxA === copy.length - 1 && idxB === 0) {
      copy.push(pt);
    } else if (idxB === copy.length - 1 && idxA === 0) {
      copy.push(pt);
    } else if (idxB > idxA) {
      copy.splice(idxA + 1, 0, pt);
    } else {
      copy.splice(idxB + 1, 0, pt);
    }
    return copy;
  }

  const pointsToEditItem =
    (idPrefix: string, textPrefix: string) => (p: Point, index: number) => ({
      id: idPrefix + index,
      text: textPrefix + index,
    });

  const edgesToEditItem =
    (idPrefix: string, textPrefix: string) =>
    (e: Edge, index: number): EditItem => ({
      id: idPrefix + index,
      text: textPrefix + index,
    });


  $: editItems = $MapStore && $MapStore.map
    ? [
        $MapStore.map.perimeter.points.map(
          pointsToEditItem("map-0-perimeter-point-", "Perimeter point #")
        ),
        pointsToEdges($MapStore.map.perimeter.points).map(
          edgesToEditItem("map-0-perimeter-edge-", "Perimeter edge #")
        ),
        ...$MapStore.map.exclusions.map((e, i) => [
          ...e.points.map(
            pointsToEditItem(
              `map-0-exclusion-${i}-point-`,
              `Exclution #${i} point #`
            )
          ),
          ...pointsToEdges(e.points).map(
            edgesToEditItem(`map-0-exclusion-${i}-edge-`, `Exclusion #${i} edge #`)
          ),
        ]),
      ].reduce((a, b) => [...a, ...b], [])
    : [];

  function shouldFilterItem(item: EditItem, value: string) {
    if (!value) return true;
    return item.text.toLowerCase().includes(value.toLowerCase());
  }

  let uiEditId: null | string = null;

  function updateButtonAvailability() {
    editPoint = edit && !!editItemId && editItemId.indexOf("-point-") !== -1;
    editEdge = edit && !!editItemId && editItemId.indexOf("-edge-") !== -1;
  }

  function selectEditItem(e: any) {
    uiEditId = editItemId = e["detail"]["selectedId"];
    updateButtonAvailability();
  }

  function clearEditItem() {
    editItemId = null;
  }

  function onDeleteClick() {
    if (editItemId != null) {
      if (editPoint) {
        let index = parseInt(editItemId.replace(/.*-point-([0-9]+)/, "$1"));
        if (editItemId.indexOf("-perimeter-") !== -1) {
          $MapStore.map.perimeter.points.splice(index, 1);
          console.log("delete point", editItemId, index, $MapStore.map.perimeter.points);
        } else {
          let exclusion = parseInt(editItemId.replace(/.*-exclusion-([0-9]+).*/, "$1"));
          $MapStore.map.exclusions[exclusion].points.splice(index, 1);
          console.log("delete point", editItemId, index, $MapStore.map.perimeter.points);
        }
        $MapStore.map.perimeter = $MapStore.map.perimeter;
      }
    }
  }

  function onSplitClick() {
    if (!editEdge || !editItemId) return;

    const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
    if (!edgeMatch) return;

    const prefix = edgeMatch[1];
    const edgeIndex = parseInt(edgeMatch[2]);

    function insertMidpoint(pts: Point[], idx: number) {
      const begin = pts[idx];
      const end = pts[(idx + 1) % pts.length];
      const newPoint = { x: (begin.x + end.x) / 2, y: (begin.y + end.y) / 2 };
      pts.splice(idx + 1, 0, newPoint);
    }

    if (prefix.includes("-perimeter")) {
      const pts = $MapStore.map.perimeter.points;
      if (edgeIndex >= pts.length) return;
      insertMidpoint(pts, edgeIndex);
      $MapStore.map.perimeter = $MapStore.map.perimeter;
    } else if (prefix.includes("-exclusion-")) {
      const exclMatch = prefix.match(/exclusion-([0-9]+)/);
      if (!exclMatch) return;
      const exclIndex = parseInt(exclMatch[1]);
      const pts = $MapStore.map.exclusions[exclIndex].points;
      if (edgeIndex >= pts.length) return;
      insertMidpoint(pts, edgeIndex);
      $MapStore.map.exclusions[exclIndex] = $MapStore.map.exclusions[exclIndex];
    } else if (prefix.includes("-dockpoints")) {
      const pts = $MapStore.map.dockpoints.points;
      if (edgeIndex >= pts.length) return;
      insertMidpoint(pts, edgeIndex);
      $MapStore.map.dockpoints = $MapStore.map.dockpoints;
    } else if (prefix.includes("-waypoints")) {
      const pts = $MapStore.map.waypoints.points;
      if (edgeIndex >= pts.length) return;
      insertMidpoint(pts, edgeIndex);
      $MapStore.map.waypoints = $MapStore.map.waypoints;
    }

    const newPointId = prefix + "-point-" + (edgeIndex + 1);
    editItemId = newPointId;
    selectedId = newPointId;
    updateButtonAvailability();
  }

  function findNearestEdgeIdx(pt: Point, pts: Point[]): number {
    let bestIdx = 0;
    let bestDist = Infinity;
    const n = pts.length;
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n;
      const ax = pts[i].x, ay = pts[i].y;
      const bx = pts[j].x, by = pts[j].y;
      const dx = bx - ax, dy = by - ay;
      const len2 = dx * dx + dy * dy;
      if (len2 < 1e-6) continue;
      let t = ((pt.x - ax) * dx + (pt.y - ay) * dy) / len2;
      t = Math.max(0, Math.min(1, t));
      const px = ax + t * dx;
      const py = ay + t * dy;
      const d = (pt.x - px) * (pt.x - px) + (pt.y - py) * (pt.y - py);
      if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    return bestIdx;
  }

  function onAddClick() {
    if (!mowerPos) return;
    const gpsPt: Point = { x: mowerPos.x, y: -mowerPos.y };
    const pts = $MapStore.map.perimeter.points;
    let newIdx: number;

    if (pts.length === 0) {
      $MapStore.map.perimeter = { points: [gpsPt] };
      newIdx = 0;
    } else {
      if (editEdge && editItemId) {
        const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
        if (edgeMatch && edgeMatch[1].includes("-perimeter")) {
          const edgeIndex = parseInt(edgeMatch[2]);
          pts.splice(edgeIndex + 1, 0, gpsPt);
          $MapStore.map.perimeter = $MapStore.map.perimeter;
          newIdx = edgeIndex + 1;
          selectNewPoint(newIdx);
          return;
        }
      }
      const idx = findNearestEdgeIdx(gpsPt, pts);
      pts.splice(idx + 1, 0, gpsPt);
      $MapStore.map.perimeter = $MapStore.map.perimeter;
      newIdx = idx + 1;
    }
    selectNewPoint(newIdx);
  }

  function selectNewPoint(index: number) {
    const id = `map-0-perimeter-point-${index}`;
    uiEditId = id;
    editItemId = id;
    selectedId = id;
    updateButtonAvailability();
  }

  function stopDraw() {
    if (!drawActive) return;
    if (floatingPoint && drawArea) {
      const pts = removePoint(getPoints(drawArea, drawExclusionIndex), floatingPoint);
      setPoints(pts, drawArea, drawExclusionIndex);
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

  function onDrawClick() {
    if (drawActive) {
      stopDraw();
      return;
    }

    if (!editEdge || !editItemId) return;

    const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
    if (!edgeMatch) return;

    const prefix = edgeMatch[1];
    const edgeIndex = parseInt(edgeMatch[2]);

    let area: 'perimeter' | 'exclusion' | 'dockpoints' | 'waypoints';
    let exclusionIndex: number | undefined;

    if (prefix.includes("-perimeter")) area = 'perimeter';
    else if (prefix.includes("-exclusion-")) {
      const exclMatch = prefix.match(/exclusion-([0-9]+)/);
      if (!exclMatch) return;
      exclusionIndex = parseInt(exclMatch[1]);
      area = 'exclusion';
    } else if (prefix.includes("-dockpoints")) area = 'dockpoints';
    else if (prefix.includes("-waypoints")) area = 'waypoints';
    else return;

    const pts = getPoints(area, exclusionIndex);
    const n = pts.length;
    if (edgeIndex >= n) return;

    const endIdx = (edgeIndex + 1) % n;
    if (endIdx === 0 && area !== 'perimeter' && area !== 'exclusion') return;

    const begin = pts[edgeIndex];
    const end = pts[endIdx];
    const midPoint = { x: (begin.x + end.x) / 2, y: (begin.y + end.y) / 2 };

    const newPts = [...pts];
    newPts.splice(edgeIndex + 1, 0, midPoint);
    setPoints(newPts, area, exclusionIndex);

    drawActive = true;
    drawArea = area;
    drawExclusionIndex = exclusionIndex;
    floatingPoint = midPoint;
    drawCandidates = [
      { area, exclusionIndex, begin, end: midPoint },
      { area, exclusionIndex, begin: midPoint, end }
    ];

    const newPointId = prefix + "-point-" + (edgeIndex + 1);
    editItemId = newPointId;
    selectedId = newPointId;
    updateButtonAvailability();
  }

  function findBestCandidate(x: number, y: number) {
    let best: typeof drawCandidates[0] | null = null;
    let bestDist = Infinity;
    for (const c of drawCandidates) {
      const mx = (c.begin.x + c.end.x) / 2;
      const my = (c.begin.y + c.end.y) / 2;
      const d = Math.hypot(x - mx, y - my);
      if (d < bestDist) { bestDist = d; best = c; }
    }
    return best;
  }

  function currentFloatingCandidates(ref: Point) {
    return drawCandidates.filter(c => c.begin === ref || c.end === ref);
  }

  function moveFloatingPoint(x: number, y: number) {
    if (!floatingPoint || !drawArea) return;
    const oldPt = floatingPoint;
    const newPt = { x, y };

    // Im Array ersetzen (verwende drawArea, nicht drawCandidates)
    const pts = [...getPoints(drawArea, drawExclusionIndex)];
    const idx = pts.indexOf(oldPt);
    if (idx !== -1) {
      pts[idx] = newPt;
      setPoints(pts, drawArea, drawExclusionIndex);
    }

    // drawCandidates aktualisieren
    drawCandidates = drawCandidates.map(c => ({
      area: c.area,
      exclusionIndex: c.exclusionIndex,
      begin: c.begin === oldPt ? newPt : c.begin,
      end: c.end === oldPt ? newPt : c.end,
    }));

    floatingPoint = newPt;
  }

  function switchToCandidate(candidate: typeof drawCandidates[0]) {
    if (!floatingPoint || !drawArea) return;
    const oldPt = floatingPoint;

    // 1. floatingPoint aus dem alten Array entfernen
    const srcPts = removePoint(getPoints(drawArea, drawExclusionIndex), oldPt);
    setPoints(srcPts, drawArea, drawExclusionIndex);

    // 2. floatingPoint in die neue Kante einfügen
    const { area, exclusionIndex, begin, end } = candidate;
    const tgtPts = insertBetween(getPoints(area, exclusionIndex), begin, end, oldPt);
    setPoints(tgtPts, area, exclusionIndex);

    // 3. State aktualisieren
    drawArea = area;
    drawExclusionIndex = exclusionIndex;

    const oldOnes = drawCandidates.filter(c => c.begin === oldPt || c.end === oldPt);
    drawCandidates = [
      ...drawCandidates.filter(c => c !== candidate && !oldOnes.includes(c)),
      { area, exclusionIndex, begin: candidate.begin, end: oldPt },
      { area, exclusionIndex, begin: oldPt, end: candidate.end }
    ];
  }

  function onDrawMapClick(event: CustomEvent<{ x: number; y: number }>) {
    if (!drawActive || !floatingPoint || drawCandidates.length === 0) return;
    const { x, y } = event.detail;

    const best = findBestCandidate(x, y);
    if (!best) return;

    const { area, exclusionIndex, begin, end } = best;
    const oldFloating = floatingPoint;

    // Neuer floatingPoint in die Mitte der besten Kante
    const newFloating = {
      x: (begin.x + end.x) / 2,
      y: (begin.y + end.y) / 2,
    };

    const pts = insertBetween(getPoints(area, exclusionIndex), begin, end, newFloating);
    setPoints(pts, area, exclusionIndex);

    drawArea = area;
    drawExclusionIndex = exclusionIndex;
    drawCandidates = [
      ...drawCandidates.filter(c => c !== best && c.begin !== oldFloating && c.end !== oldFloating),
      { area, exclusionIndex, begin, end: newFloating },
      { area, exclusionIndex, begin: newFloating, end }
    ];
    floatingPoint = newFloating;
  }

  function orient(ax: number, ay: number, bx: number, by: number, cx: number, cy: number): number {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  }

  function segmentsIntersect(a1: Point, a2: Point, b1: Point, b2: Point): boolean {
    const EPS = 1e-9;
    const o1 = orient(a1.x, a1.y, a2.x, a2.y, b1.x, b1.y);
    const o2 = orient(a1.x, a1.y, a2.x, a2.y, b2.x, b2.y);
    const o3 = orient(b1.x, b1.y, b2.x, b2.y, a1.x, a1.y);
    const o4 = orient(b1.x, b1.y, b2.x, b2.y, a2.x, a2.y);

    // Kollinearer Fall ignorieren
    if (Math.abs(o1) < EPS && Math.abs(o2) < EPS && Math.abs(o3) < EPS && Math.abs(o4) < EPS) {
      return false;
    }

    const straddle1 = (o1 > EPS && o2 < -EPS) || (o1 < -EPS && o2 > EPS);
    const straddle2 = (o3 > EPS && o4 < -EPS) || (o3 < -EPS && o4 > EPS);

    return straddle1 && straddle2;
  }

  function edgesFromPoints(pts: Point[], closed: boolean): Array<{begin: Point, end: Point}> {
    const edges = [];
    const n = pts.length;
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n;
      if (!closed && j === 0) break;
      edges.push({ begin: pts[i], end: pts[j] });
    }
    return edges;
  }

  function findCrossedCandidate(x: number, y: number): typeof drawCandidates[0] | null {
    if (!floatingPoint || !drawArea) return null;

    const oldPt = floatingPoint;
    const pts = getPoints(drawArea, drawExclusionIndex);
    const isClosed = drawArea === 'perimeter' || drawArea === 'exclusion';
    const allEdges = edgesFromPoints(pts, isClosed);

    // Aktuelle Nachbarn finden
    const currentNeighbors = drawCandidates
      .filter(c => c.begin === oldPt || c.end === oldPt)
      .map(c => c.begin === oldPt ? c.end : c.begin);

    let crossed: typeof drawCandidates[0] | null = null;

    for (const neighbor of currentNeighbors) {
      for (const other of allEdges) {
        // Eigene Kante ausschließen
        if (other.begin === neighbor || other.end === neighbor) continue;
        // Kanten mit oldPt als Endpunkt ausschließen
        if (other.begin === oldPt || other.end === oldPt) continue;

        if (segmentsIntersect({ x, y }, neighbor, other.begin, other.end)) {
          const inCandidates = drawCandidates.find(dc =>
            (dc.begin === other.begin && dc.end === other.end) ||
            (dc.begin === other.end && dc.end === other.begin)
          );
          crossed = inCandidates || { area: drawArea, exclusionIndex: drawExclusionIndex, begin: other.begin, end: other.end };
          break;
        }
      }
      if (crossed) break;
    }

    return crossed;
  }

  function onMouseMove(event: CustomEvent<{ x: number; y: number }>) {
    if (!drawActive || !floatingPoint) return;
    const { x, y } = event.detail;

    const crossed = findCrossedCandidate(x, y);
    if (crossed) {
      switchToCandidate(crossed);
    }

    moveFloatingPoint(x, y);
  }

  // ─── Sync-Status: CRC aus Metadaten (Backend-berechnet) ─────────
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
      rotation: compassRotation,
    });
  } else if (edit) {
    wasEditing = true;
  }

  // ─── Goto ────────────────────────────────────────────────────────────────

  let targetSet = false;
  let targetPos: { x: number; y: number } | null = null;
  let isDriving = false;
  let targetDist = 0;
  let targetBearing = 0;

  $: hasMap = $MapStore?.map?.perimeter?.points?.length > 0;
  $: mowerPos = $socketStore.state?.position ?? null;

  $: if (mowerPos && targetPos) {
    const mx = mowerPos.x;
    const my = -mowerPos.y;
    const tx = targetPos.x;
    const ty = targetPos.y;
    const dx = tx - mx;
    const dy = ty - my;
    targetDist = Math.sqrt(dx * dx + dy * dy);
    targetBearing = (Math.atan2(dx, -dy) * 180) / Math.PI;
  }

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
    // y from d3.pointer is in viewBox space, where positive=down.
    // The robot's pos.y is in world space, where positive=north.
    // The p2p function negates Y for display: viewBoxY = -worldY.
    // So targetY in world space = -viewBoxY for the modem (which sends perimeter coords).
    // But we display in viewBox space, so store viewBox coords.
    console.log('[map] click', 'viewBox x:', x, 'y:', y, '-> navigateTo(', x, ',', -y, ')');
    targetPos = { x, y };
    targetSet = true;
    isDriving = false;
    targetDist = 0;
    targetBearing = 0;
  }

  function startDrive() {
    if (!targetSet || !targetPos) return;
    // modem expects perimeter/world coordinates, so negate Y
    socketService.sendNavigateTo(targetPos.x, -targetPos.y);
    isDriving = true;
  }

  function stopDrive() {
    socketService.sendJoystickMove(0, 0);
    isDriving = false;
  }

  function clearTarget() {
    socketService.sendJoystickMove(0, 0);
    isDriving = false;
    targetSet = false;
    targetPos = null;
    targetDist = 0;
    targetBearing = 0;
  }

  let compassRotation = 0;
  let rotationManuallySet = false;
  let lastRotationMapId: string | null = null;
  $: if ($socketStore.currentMapId !== lastRotationMapId) {
    lastRotationMapId = $socketStore.currentMapId;
    rotationManuallySet = false;
  }
  $: if (!rotationManuallySet && $socketStore.currentMapMeta?.rotation !== undefined) {
    const r = $socketStore.currentMapMeta.rotation;
    if (compassRotation !== r) compassRotation = r;
  }
  function onCompassDown(event: MouseEvent) {
    rotationManuallySet = true;
    event.preventDefault();
    const btn = event.currentTarget as HTMLElement;
    const rect = btn.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const startAngle = Math.atan2(event.clientY - centerY, event.clientX - centerX) * 180 / Math.PI;
    let lastAngle = startAngle;
    let didDrag = false;

    function onMove(e: MouseEvent) {
      didDrag = true;
      const angle = Math.atan2(e.clientY - centerY, e.clientX - centerX) * 180 / Math.PI;
      let delta = angle - lastAngle;
      if (delta > 180) delta -= 360;
      if (delta < -180) delta += 360;
      compassRotation = ((compassRotation + delta) % 360 + 360) % 360;
      lastAngle = angle;
    }

    function onUp() {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      if (!didDrag) {
        compassRotation = ((compassRotation + 90) % 360 + 360) % 360;
      }
    }

    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  }
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
          <div class="toolbar-btn-row">
            {#if $mapWorkflowStore.renameMode}
              <Button
                kind="primary"
                size="small"
                disabled={!$mapWorkflowStore.pendingName || $mapWorkflowStore.pendingName === effectiveMapName}
                on:click={confirmRename}
                icon={IconCheckmark}
                iconDescription="Confirm rename"
              >
                <span class="btn-label">OK</span>
              </Button>
              <Button
                kind="secondary"
                size="small"
                on:click={cancelRename}
                icon={IconClose}
                iconDescription="Cancel rename"
              >
                <span class="btn-label">Cancel</span>
              </Button>
            {:else}
              <Button
                kind="secondary"
                size="small"
                disabled={busy}
                icon={IconUpload}
                iconDescription="Upload map to mower"
                on:click={() => socketService.sendUploadMap()}
              />
              <Button
                kind={showManage ? "primary" : "secondary"}
                size="small"
                icon={IconTools}
                iconDescription="Manage maps"
                on:click={toggleManage}
              />
              <Button
                kind={edit ? "primary" : "secondary"}
                size="small"
                icon={IconEdit}
                iconDescription="Edit map"
                on:click={toggleEdit}
              />
              <Button
                kind={showCalculate ? "primary" : "secondary"}
                size="small"
                icon={IconMagicWand}
                iconDescription="Calculate waypoints"
                on:click={toggleCalculate}
              />
            {/if}
          </div>
        </div>
        {#if !showManage && !edit && !showCalculate && !sync.needsUpload}
          <div class="toolbar-goto-group">
            {#if hasMap}
              {#if targetSet}
                <span class="goto-badge">
                  {targetDist.toFixed(1)}m / {targetBearing.toFixed(0)}°
                </span>
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
        <Row class="map-mgmt-row">
          <Column style="flex-shrink: 0;">
            <div class="toolbar-btn-row">
              <Button
                kind="primary"
                size="small"
                disabled={!isDirty || $mapWorkflowStore.renameMode || workflowBusy}
                on:click={onSaveMap}
                icon={IconSave}
                iconDescription="Save map"
              >
                <span class="btn-label">Save</span>
              </Button>
              <Button
                kind="secondary"
                size="small"
                disabled={!canRename || $mapWorkflowStore.renameMode || workflowBusy}
                icon={IconPen}
                iconDescription="Rename map"
                on:click={startRename}
              >
                <span class="btn-label">Rename</span>
              </Button>
              <Button
                kind="danger"
                size="small"
                disabled={!effectiveMapId || $mapWorkflowStore.renameMode || workflowBusy}
                on:click={onDeleteMap}
                icon={IconTrashCan}
                iconDescription="Delete map"
              >
                <span class="btn-label">Del</span>
              </Button>
              <Button
                kind="tertiary"
                size="small"
                disabled={!effectiveMapId || $mapWorkflowStore.renameMode || workflowBusy}
                on:click={onSetDefaultMap}
                icon={IconStar}
                iconDescription="Set as default map"
              >
                <span class="btn-label">Default</span>
              </Button>
              <Button
                kind="secondary"
                size="small"
                disabled={$mapWorkflowStore.renameMode || workflowBusy}
                on:click={onNewMap}
                icon={IconNew}
                iconDescription="New map"
              >
                <span class="btn-label">New</span>
              </Button>
              {#if $mapWorkflowStore.pendingName && $mapWorkflowStore.pendingName !== effectiveMapName}
                <span class="pending-map-name" title="Neuer Name, noch nicht gespeichert">→ {$mapWorkflowStore.pendingName}</span>
              {/if}
            </div>
          </Column>
        </Row>
      {/if}

      {#if edit}
        <Row class="map-edit-row">
          <div class="edit-combo">
            <ComboBox
              disabled={!edit}
              placeholder="Select item to edit"
              items={editItems}
              bind:selectedId
              on:select={selectEditItem}
              on:clear={clearEditItem}
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
                disabled={!editEdge}
                icon={IconSplit}
                iconDescription="Split"
                on:click={onSplitClick}
              />
              <Button
                kind="tertiary"
                size="small"
                disabled={!editEdge}
                icon={IconCut}
                iconDescription="Cut"
              />
              <Button
                kind="tertiary"
                size="small"
                disabled={!mowerPos || nearPos}
                icon={IconAdd}
                iconDescription="Add"
                on:click={onAddClick}
              >
                <span class="btn-label">Add</span>
              </Button>
              <Button
                kind="danger"
                size="small"
                disabled={!edit || !editPoint}
                on:click={onDeleteClick}
                icon={IconTrashCan}
                iconDescription="Delete"
              />
            </div>
          </div>
        </Row>
      {/if}

      {#if showCalculate}
        <Row class="map-calc-row">
          <Column>
            <div class="toolbar-btn-row">
              <Button
                kind="secondary"
                size="small"
                icon={IconSettings}
                iconDescription="Mow settings"
                on:click={() => { showMowSettings = true; }}
              />
              <Button
                kind="danger"
                size="small"
                disabled={busy}
                icon={IconTrashCan}
                iconDescription="Clear waypoints"
                on:click={() => socketService.sendClearWaypoints()}
              >
                <span class="btn-label">Clear WP</span>
              </Button>
              <Button
                kind="secondary"
                size="small"
                disabled={busy}
                icon={IconMagicWand}
                iconDescription="Calculate waypoints"
                on:click={() => socketService.sendCalculateWaypoints()}
              >
                <span class="btn-label">Calculate</span>
              </Button>
            </div>
          </Column>
        </Row>
      {/if}
    </Grid>
  </div>
  <div class="map-canvas-wrapper">
    <div class="map-top-right">
      <button class="compass-btn" on:mousedown={onCompassDown} title="Karte drehen (ziehen f&uuml;r feine Ausrichtung)">
        <svg viewBox="-12 -12 24 24" width="28" height="28">
          <g transform="rotate({compassRotation})">
            <circle cx="0" cy="0" r="10" fill="white" stroke="#999" stroke-width="1.5"/>
            <polygon points="0,-8 -4,0 0,-2 4,0" fill="#d32f2f"/>
            <polygon points="0,8 -4,0 0,2 4,0" fill="#999"/>
          </g>
        </svg>
      </button>
      <div class="map-point-counts">
        <div><strong>Area:</strong> {($socketStore.currentMapMeta?.area ?? 0).toFixed(1)} m²</div>
        <div><strong>Perimeter:</strong> {perimeterPoints}</div>
        {#each exclusionPoints as ep, i}
          <div><strong>Excl #{i}:</strong> {ep}</div>
        {/each}
        <div><strong>Dock:</strong> {dockpointsPoints}</div>
        <div><strong>Way:</strong> {waypointsPoints}</div>
        <div><strong>Total:</strong> {totalPoints}</div>
        <div class:sync-ok={!sync.needsUpload} class:sync-warn={sync.needsUpload}>{sync.needsUpload ? '⚠' : '✓'} {sync.needsUpload ? 'not synced' : 'synced'}</div>
      </div>
    </div>
    <Canvas compassRotation={compassRotation} on:mapclick={onMapClick} on:mousemove={onMouseMove}>
      {#if $MapStore && $MapStore.map}
        <Track />
        <Perimeter
          value={$MapStore.map.perimeter}
          perimiterId="map-0-perimeter"
          {edit}
          bind:editItemId
        />
        {#each $MapStore.map.exclusions as exclusion, index}
          <Exclusion
            value={exclusion}
            exclusionId={"map-0-exclusion-" + index}
            {edit}
            bind:editItemId
          />
        {/each}
        <Waypoints
          value={$MapStore.map.waypoints}
          waypointsId="map-0-waypoints"
          {edit}
          bind:editItemId
        />
        <Dockpoints
          value={$MapStore.map.dockpoints}
          dockpointsId="map-0-dockpoints"
          {edit}
          bind:editItemId
        />
        <MowerPosition position={mowerPos} />

        {#if drawActive && floatingPoint}
          {#each drawCandidates.filter(c => c.begin === floatingPoint || c.end === floatingPoint) as c}
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

        {#if targetSet && targetPos && !showManage && !edit && !showCalculate && !sync.needsUpload}
          <circle
            cx={targetPos.x}
            cy={targetPos.y}
            r="0.18"
            fill="none"
            stroke="#e65100"
            stroke-width="0.04"
            opacity="0.6"
          />
          <circle
            cx={targetPos.x}
            cy={targetPos.y}
            r="0.07"
            fill="#e65100"
            stroke="white"
            stroke-width="0.02"
          />
        {/if}

        {#if targetSet && targetPos && mowerPos && (mowerPos.x !== 0 || mowerPos.y !== 0) && !showManage && !edit && !showCalculate && !sync.needsUpload}
          <line
            x1={mowerPos.x}
            y1={-mowerPos.y}
            x2={targetPos.x}
            y2={targetPos.y}
            stroke="#e65100"
            stroke-width="0.04"
            stroke-dasharray="0.12, 0.12"
            opacity="0.7"
            pointer-events="none"
          />
        {/if}
      {/if}
    </Canvas>
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

    &.drive {
      background: #e8f5e9;
      border-color: #4caf50;
      color: #2e7d32;
    }

    &.stop {
      background: #ffebee;
      border-color: #ef5350;
      color: #c62828;
    }

    &.clear {
      padding: 4px 8px;
    }
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
  .toolbar-btn-row {
    display: flex;
    flex: 0 0 auto;
    flex-wrap: nowrap;
    gap: 0.25rem;
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

  .action-btns {
    display: flex;
    flex-wrap: nowrap;
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

  .pending-map-name {
    margin-left: 0.5rem;
    padding: 0.25rem 0.5rem;
    background: #fff3e0;
    color: #b06000;
    border: 1px solid #ffcc80;
    border-radius: 4px;
    font-size: 0.85em;
    font-weight: 600;
    white-space: nowrap;
    max-width: 200px;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  @container (max-width: 800px) {
    .btn-label {
      display: none;
    }
    .toolbar-main-group {
      width: 100%;
    }
    .toolbar-btn-row :global(.bx--btn) {
      width: 2rem;
      padding: 0 !important;
      justify-content: center;
    }
    .toolbar-btn-row :global(.bx--btn .bx--btn__icon) {
      position: static;
      margin: 0;
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

  .map-top-right {
    position: absolute;
    top: 8px;
    right: 8px;
    z-index: 10;
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 6px;
  }

  .compass-btn {
    width: 32px;
    height: 32px;
    border-radius: 50%;
    border: 1px solid #ccc;
    background: white;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    box-shadow: 0 1px 4px rgba(0,0,0,0.15);
    padding: 0;
    flex-shrink: 0;
  }
  .compass-btn:hover {
    background: #f5f5f5;
    box-shadow: 0 2px 6px rgba(0,0,0,0.2);
  }
  .compass-btn svg {
    display: block;
  }

  .map-point-counts {
    background: rgba(255, 255, 255, 0.85);
    border: 1px solid #ccc;
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 0.7em;
    font-family: monospace;
    color: #333;
    pointer-events: none;
  }

  .sync-ok  { color: #2e7d32; }
  .sync-warn { color: #b06000; font-weight: bold; }
</style>
