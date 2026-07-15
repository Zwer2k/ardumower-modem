import type { Point, Edge } from "../model";
import { MapStore, cloneMap } from "../service";
import { pointsToEdges } from "../core/geometry";
import { get } from "svelte/store";
import { setMapDirty } from "../services/map-sync";

export function getPoints(
  map: import("../model").Map,
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints",
  exclusionIndex?: number
): Point[] {
  if (area === "perimeter") return map.perimeter.points;
  if (area === "exclusion") return map.exclusions[exclusionIndex!].points;
  if (area === "dockpoints") return map.dockpoints.points;
  return map.waypoints.points;
}

export function setPoints(
  points: Point[],
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints",
  exclusionIndex?: number
) {
  MapStore.update((store) => {
    const map = { ...store.map };
    if (area === "perimeter") {
      map.perimeter = { ...map.perimeter, points };
    } else if (area === "exclusion") {
      const exclusions = [...map.exclusions];
      exclusions[exclusionIndex!] = { ...exclusions[exclusionIndex!], points };
      map.exclusions = exclusions;
    } else if (area === "dockpoints") {
      map.dockpoints = { ...map.dockpoints, points };
    } else {
      map.waypoints = { ...map.waypoints, points };
    }
    return { ...store, map };
  });
  setMapDirty(true);
}

export function removePoint(pts: Point[], pt: Point): Point[] {
  const idx = pts.indexOf(pt);
  if (idx !== -1) {
    const copy = [...pts];
    copy.splice(idx, 1);
    return copy;
  }
  return [...pts];
}

export function insertBetween(pts: Point[], a: Point, b: Point, pt: Point): Point[] {
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

export function insertMidpoint(pts: Point[], idx: number): Point[] {
  const begin = pts[idx];
  const end = pts[(idx + 1) % pts.length];
  const newPoint = { x: (begin.x + end.x) / 2, y: (begin.y + end.y) / 2 };
  const copy = [...pts];
  copy.splice(idx + 1, 0, newPoint);
  return copy;
}

export function findNearestEdgeIdx(pt: Point, pts: Point[]): number {
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

export function edgesFromPoints(
  pts: Point[],
  closed: boolean
): Array<{ begin: Point; end: Point }> {
  const edges = [];
  const n = pts.length;
  for (let i = 0; i < n; i++) {
    const j = (i + 1) % n;
    if (!closed && j === 0) break;
    edges.push({ begin: pts[i], end: pts[j] });
  }
  return edges;
}

export function orient(
  ax: number, ay: number,
  bx: number, by: number,
  cx: number, cy: number
): number {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

export function segmentsIntersect(a1: Point, a2: Point, b1: Point, b2: Point): boolean {
  const EPS = 1e-9;
  const o1 = orient(a1.x, a1.y, a2.x, a2.y, b1.x, b1.y);
  const o2 = orient(a1.x, a1.y, a2.x, a2.y, b2.x, b2.y);
  const o3 = orient(b1.x, b1.y, b2.x, b2.y, a1.x, a1.y);
  const o4 = orient(b1.x, b1.y, b2.x, b2.y, a2.x, a2.y);

  if (Math.abs(o1) < EPS && Math.abs(o2) < EPS && Math.abs(o3) < EPS && Math.abs(o4) < EPS) {
    return false;
  }

  const straddle1 = (o1 > EPS && o2 < -EPS) || (o1 < -EPS && o2 > EPS);
  const straddle2 = (o3 > EPS && o4 < -EPS) || (o3 < -EPS && o4 > EPS);

  return straddle1 && straddle2;
}

export interface DrawCandidate {
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints";
  exclusionIndex?: number;
  begin: Point;
  end: Point;
}

export function findBestCandidate(
  x: number,
  y: number,
  drawCandidates: DrawCandidate[]
): DrawCandidate | null {
  let best: DrawCandidate | null = null;
  let bestDist = Infinity;
  for (const c of drawCandidates) {
    const mx = (c.begin.x + c.end.x) / 2;
    const my = (c.begin.y + c.end.y) / 2;
    const d = Math.hypot(x - mx, y - my);
    if (d < bestDist) { bestDist = d; best = c; }
  }
  return best;
}

export function findCrossedCandidate(
  x: number,
  y: number,
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  floatingPoint: Point | null,
  drawCandidates: DrawCandidate[]
): DrawCandidate | null {
  if (!floatingPoint || !drawArea) return null;

  const oldPt = floatingPoint;
  const pts = getPoints(get(MapStore).map, drawArea, drawExclusionIndex);
  const isClosed = drawArea === "perimeter" || drawArea === "exclusion";
  const allEdges = edgesFromPoints(pts, isClosed);

  const currentNeighbors = drawCandidates
    .filter((c) => c.begin === oldPt || c.end === oldPt)
    .map((c) => (c.begin === oldPt ? c.end : c.begin));

  let crossed: DrawCandidate | null = null;

  for (const neighbor of currentNeighbors) {
    for (const other of allEdges) {
      if (other.begin === neighbor || other.end === neighbor) continue;
      if (other.begin === oldPt || other.end === oldPt) continue;

      if (segmentsIntersect({ x, y }, neighbor, other.begin, other.end)) {
        const inCandidates = drawCandidates.find(
          (dc) =>
            (dc.begin === other.begin && dc.end === other.end) ||
            (dc.begin === other.end && dc.end === other.begin)
        );
        crossed =
          inCandidates || {
            area: drawArea,
            exclusionIndex: drawExclusionIndex,
            begin: other.begin,
            end: other.end,
          };
        break;
      }
    }
    if (crossed) break;
  }

  return crossed;
}

export function pointsToEditItem(idPrefix: string, textPrefix: string) {
  return (p: Point, index: number) => ({
    id: idPrefix + index,
    text: textPrefix + index,
  });
}

export function edgesToEditItem(idPrefix: string, textPrefix: string) {
  return (e: Edge, index: number) => ({
    id: idPrefix + index,
    text: textPrefix + index,
  });
}

export interface EditItem {
  id: string;
  text: string;
}

export function buildEditItems(map: import("../model").Map): EditItem[] {
  if (!map) return [];
  return [
    map.perimeter.points.map(pointsToEditItem("map-0-perimeter-point-", "Perimeter point #")),
    pointsToEdges(map.perimeter.points).map(edgesToEditItem("map-0-perimeter-edge-", "Perimeter edge #")),
    ...map.exclusions.map((e, i) => [
      ...e.points.map(pointsToEditItem(`map-0-exclusion-${i}-point-`, `Exclusion #${i} point #`)),
      ...pointsToEdges(e.points).map(edgesToEditItem(`map-0-exclusion-${i}-edge-`, `Exclusion #${i} edge #`)),
    ]),
  ].reduce((a, b) => [...a, ...b], [] as EditItem[]);
}

export function deletePointByEditItemId(editItemId: string) {
  if (editItemId.indexOf("-perimeter-") !== -1) {
    const index = parseInt(editItemId.replace(/.*-point-([0-9]+)/, "$1"));
    MapStore.update((store) => {
      const map = { ...store.map, perimeter: { ...store.map.perimeter } };
      map.perimeter.points = [...map.perimeter.points];
      map.perimeter.points.splice(index, 1);
      return { ...store, map };
    });
  } else if (editItemId.indexOf("-exclusion-") !== -1) {
    const exclusion = parseInt(editItemId.replace(/.*-exclusion-([0-9]+).*/, "$1"));
    const index = parseInt(editItemId.replace(/.*-point-([0-9]+)/, "$1"));
    MapStore.update((store) => {
      const map = { ...store.map };
      const exclusions = [...map.exclusions];
      exclusions[exclusion] = { ...exclusions[exclusion] };
      exclusions[exclusion].points = [...exclusions[exclusion].points];
      exclusions[exclusion].points.splice(index, 1);
      map.exclusions = exclusions;
      return { ...store, map };
    });
  }
  setMapDirty(true);
}

export function splitEdgeByEditItemId(editItemId: string): string | null {
  const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
  if (!edgeMatch) return null;

  const prefix = edgeMatch[1];
  const edgeIndex = parseInt(edgeMatch[2]);

  if (prefix.includes("-perimeter")) {
    MapStore.update((store) => {
      const map = { ...store.map };
      map.perimeter = { ...map.perimeter, points: insertMidpoint(map.perimeter.points, edgeIndex) };
      return { ...store, map };
    });
  } else if (prefix.includes("-exclusion-")) {
    const exclMatch = prefix.match(/exclusion-([0-9]+)/);
    if (!exclMatch) return null;
    const exclIndex = parseInt(exclMatch[1]);
    MapStore.update((store) => {
      const map = { ...store.map };
      const exclusions = [...map.exclusions];
      exclusions[exclIndex] = { ...exclusions[exclIndex], points: insertMidpoint(exclusions[exclIndex].points, edgeIndex) };
      map.exclusions = exclusions;
      return { ...store, map };
    });
  } else if (prefix.includes("-dockpoints")) {
    MapStore.update((store) => {
      const map = { ...store.map };
      map.dockpoints = { ...map.dockpoints, points: insertMidpoint(map.dockpoints.points, edgeIndex) };
      return { ...store, map };
    });
  } else if (prefix.includes("-waypoints")) {
    MapStore.update((store) => {
      const map = { ...store.map };
      map.waypoints = { ...map.waypoints, points: insertMidpoint(map.waypoints.points, edgeIndex) };
      return { ...store, map };
    });
  } else {
    return null;
  }
  setMapDirty(true);

  return prefix + "-point-" + (edgeIndex + 1);
}

export function buildPointId(editItemId: string | null, index: number): string | null {
  if (!editItemId) return null;
  const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
  const pointMatch = editItemId.match(/^(.*)-point-([0-9]+)$/);
  const prefix = edgeMatch ? edgeMatch[1] : pointMatch ? pointMatch[1] : null;
  if (!prefix) return null;
  return prefix + "-point-" + index;
}

export function buildInitialCandidates(
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints",
  exclusionIndex: number | undefined,
  begin: Point,
  end: Point,
  midPoint: Point
): DrawCandidate[] {
  return [
    { area, exclusionIndex, begin, end: midPoint },
    { area, exclusionIndex, begin: midPoint, end },
  ];
}

export function addPointAtMowerPosition(mowerPos: { x: number; y: number } | null, editItemId: string | null, editEdge: boolean): number | null {
  if (!mowerPos) return null;
  const gpsPt: Point = { x: mowerPos.x, y: -mowerPos.y };
  const perimeter = get(MapStore).map.perimeter;

  if (perimeter.points.length === 0) {
    MapStore.update((store) => ({
      ...store,
      map: { ...store.map, perimeter: { points: [gpsPt] } },
    }));
    setMapDirty(true);
    return 0;
  }

  if (editEdge && editItemId) {
    const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
    if (edgeMatch && edgeMatch[1].includes("-perimeter")) {
      const edgeIndex = parseInt(edgeMatch[2]);
      MapStore.update((store) => {
        const map = { ...store.map };
        map.perimeter = { ...map.perimeter, points: [...map.perimeter.points] };
        map.perimeter.points.splice(edgeIndex + 1, 0, gpsPt);
        return { ...store, map };
      });
      setMapDirty(true);
      return edgeIndex + 1;
    }
  }

  const idx = findNearestEdgeIdx(gpsPt, perimeter.points);
  MapStore.update((store) => {
    const map = { ...store.map };
    map.perimeter = { ...map.perimeter, points: [...map.perimeter.points] };
    map.perimeter.points.splice(idx + 1, 0, gpsPt);
    return { ...store, map };
  });
  setMapDirty(true);
  return idx + 1;
}

export function parseAreaFromEditItemId(editItemId: string): {
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints";
  exclusionIndex?: number;
} | null {
  const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
  if (!edgeMatch) return null;
  const prefix = edgeMatch[1];

  if (prefix.includes("-perimeter")) return { area: "perimeter" };
  if (prefix.includes("-exclusion-")) {
    const exclMatch = prefix.match(/exclusion-([0-9]+)/);
    if (!exclMatch) return null;
    return { area: "exclusion", exclusionIndex: parseInt(exclMatch[1]) };
  }
  if (prefix.includes("-dockpoints")) return { area: "dockpoints" };
  if (prefix.includes("-waypoints")) return { area: "waypoints" };
  return null;
}

export function startDrawMode(
  editItemId: string | null
): {
  area: "perimeter" | "exclusion" | "dockpoints" | "waypoints";
  exclusionIndex?: number;
  begin: Point;
  end: Point;
  midPoint: Point;
  newPointId: string;
} | null {
  if (!editItemId) return null;
  const parsed = parseAreaFromEditItemId(editItemId);
  if (!parsed) return null;
  const { area, exclusionIndex } = parsed;

  const edgeMatch = editItemId.match(/^(.*)-edge-([0-9]+)$/);
  if (!edgeMatch) return null;
  const prefix = edgeMatch[1];
  const edgeIndex = parseInt(edgeMatch[2]);

  const pts = getPoints(get(MapStore).map, area, exclusionIndex);
  const n = pts.length;
  if (edgeIndex >= n) return null;

  const endIdx = (edgeIndex + 1) % n;
  if (endIdx === 0 && area !== "perimeter" && area !== "exclusion") return null;

  const begin = pts[edgeIndex];
  const end = pts[endIdx];
  const midPoint = { x: (begin.x + end.x) / 2, y: (begin.y + end.y) / 2 };

  const newPts = [...pts];
  newPts.splice(edgeIndex + 1, 0, midPoint);
  setPoints(newPts, area, exclusionIndex);

  return { area, exclusionIndex, begin, end, midPoint, newPointId: prefix + "-point-" + (edgeIndex + 1) };
}

export function moveFloatingPoint(
  x: number,
  y: number,
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  floatingPoint: Point | null,
  drawCandidates: DrawCandidate[]
): { newFloatingPoint: Point; newCandidates: DrawCandidate[] } | null {
  if (!floatingPoint || !drawArea) return null;
  const oldPt = floatingPoint;
  const newPt = { x, y };

  const pts = [...getPoints(get(MapStore).map, drawArea, drawExclusionIndex)];
  const idx = pts.indexOf(oldPt);
  if (idx !== -1) {
    pts[idx] = newPt;
    setPoints(pts, drawArea, drawExclusionIndex);
  }

  const newCandidates = drawCandidates.map((c) => ({
    area: c.area,
    exclusionIndex: c.exclusionIndex,
    begin: c.begin === oldPt ? newPt : c.begin,
    end: c.end === oldPt ? newPt : c.end,
  }));

  return { newFloatingPoint: newPt, newCandidates };
}

export function switchToCandidate(
  candidate: DrawCandidate,
  floatingPoint: Point | null,
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  drawCandidates: DrawCandidate[]
): {
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints";
  drawExclusionIndex: number | undefined;
  drawCandidates: DrawCandidate[];
  floatingPoint: Point;
} | null {
  if (!floatingPoint || !drawArea) return null;
  const oldPt = floatingPoint;

  const srcPts = removePoint(getPoints(get(MapStore).map, drawArea, drawExclusionIndex), oldPt);
  setPoints(srcPts, drawArea, drawExclusionIndex);

  const { area, exclusionIndex, begin, end } = candidate;
  const tgtPts = insertBetween(getPoints(get(MapStore).map, area, exclusionIndex), begin, end, oldPt);
  setPoints(tgtPts, area, exclusionIndex);

  const oldOnes = drawCandidates.filter((c) => c.begin === oldPt || c.end === oldPt);
  const newCandidates = [
    ...drawCandidates.filter((c) => c !== candidate && !oldOnes.includes(c)),
    { area, exclusionIndex, begin: candidate.begin, end: oldPt },
    { area, exclusionIndex, begin: oldPt, end: candidate.end },
  ];

  return { drawArea: area, drawExclusionIndex: exclusionIndex, drawCandidates: newCandidates, floatingPoint: oldPt };
}

export function placeFloatingPoint(
  x: number,
  y: number,
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  floatingPoint: Point | null,
  drawCandidates: DrawCandidate[]
): {
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints";
  drawExclusionIndex: number | undefined;
  drawCandidates: DrawCandidate[];
  floatingPoint: Point;
} | null {
  if (!drawArea || !floatingPoint || drawCandidates.length === 0) return null;
  const best = findBestCandidate(x, y, drawCandidates);
  if (!best) return null;

  const { area, exclusionIndex, begin, end } = best;
  const oldFloating = floatingPoint;
  const newFloating = {
    x: (begin.x + end.x) / 2,
    y: (begin.y + end.y) / 2,
  };

  const pts = insertBetween(getPoints(get(MapStore).map, area, exclusionIndex), begin, end, newFloating);
  setPoints(pts, area, exclusionIndex);

  const newCandidates = [
    ...drawCandidates.filter((c) => c !== best && c.begin !== oldFloating && c.end !== oldFloating),
    { area, exclusionIndex, begin, end: newFloating },
    { area, exclusionIndex, begin: newFloating, end },
  ];

  return { drawArea: area, drawExclusionIndex: exclusionIndex, drawCandidates: newCandidates, floatingPoint: newFloating };
}

export function findPointIndex(
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  point: Point
): number {
  if (!drawArea) return -1;
  const pts = getPoints(get(MapStore).map, drawArea, drawExclusionIndex);
  return pts.indexOf(point);
}

export function removeFloatingPoint(
  drawArea: "perimeter" | "exclusion" | "dockpoints" | "waypoints" | null,
  drawExclusionIndex: number | undefined,
  floatingPoint: Point | null
) {
  if (!drawArea || !floatingPoint) return;
  const pts = removePoint(getPoints(get(MapStore).map, drawArea, drawExclusionIndex), floatingPoint);
  setPoints(pts, drawArea, drawExclusionIndex);
}

