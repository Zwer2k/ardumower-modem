import type { Point, Edge } from "./model"

export const pointsForPolygon = (p: Point[]): string => p.map(({ x, y }) => `${x},${y}`).join(' ')

export const rotatePointsAroundOrigin = (p: Point[], radians: number): Point[] =>
  ((s: number, c: number) =>
    p.map(({ x, y }) =>
      ({ x: x * c - y * s, y: x * s + y * c })))
    (Math.sin(radians), Math.cos(radians))

export const pointsToEdges = (p: Point[]): Edge[] =>
  p.length < 2
    ? []
    : [...p, p[0]]
      .map((_, i, a) => (i === 0 ? undefined : [a[i - 1], a[i]]))
      .filter((x) => x !== undefined)
      .map((p) => ({ begin: p[0], end: p[1] }));

// V-förmige Pfeile an den Mittelpunkten aller Kanten eines Polylinienzuges.
// size: Länge des Pfeilschenkels (vom Mittelpunkt bis zum Ende), 
// hw: halbe Breite (Öffnung des V).
export function edgeArrowPath(points: Point[], size: number, hw: number): string {
  let d = '';
  for (let i = 1; i < points.length; i++) {
    d += arrowAt(points[i-1], points[i], size, hw);
  }
  return d;
}

// Wie edgeArrowPath, schließt aber die letzte Kante zum ersten Punkt (Polygon).
export function polygonArrowPath(points: Point[], size: number, hw: number): string {
  let d = edgeArrowPath(points, size, hw);
  if (points.length >= 3) {
    d += arrowAt(points[points.length-1], points[0], size, hw);
  }
  return d;
}

function arrowAt(p1: Point, p2: Point, size: number, hw: number): string {
  const dx = p2.x - p1.x;
  const dy = p2.y - p1.y;
  const len = Math.sqrt(dx * dx + dy * dy);
  if (len < 0.001) return '';
  const mx = (p1.x + p2.x) / 2;
  const my = (p1.y + p2.y) / 2;
  const cos = dx / len;
  const sin = dy / len;
  // V-Schenkel: (-size, -hw) und (-size, hw) im lokalen Koordinatensystem,
  // rotiert um angle und verschoben zum Mittelpunkt
  const l1x = mx + (-size * cos - (-hw) * sin);
  const l1y = my + (-size * sin + (-hw) * cos);
  const l2x = mx + (-size * cos - hw * sin);
  const l2y = my + (-size * sin + hw * cos);
  return `M ${l1x} ${l1y} L ${mx} ${my} L ${l2x} ${l2y} `;
}
