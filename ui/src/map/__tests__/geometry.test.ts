import { describe, it, expect } from "vitest";
import { pointsForPolygon, rotatePointsAroundOrigin, pointsToEdges, edgeArrowPath, polygonArrowPath } from "../geometry";
import type { Point } from "../model";

describe("pointsForPolygon", () => {
  it("should join points as x,y pairs", () => {
    const points: Point[] = [{ x: 1, y: 2 }, { x: 3, y: 4 }];
    expect(pointsForPolygon(points)).toBe("1,2 3,4");
  });
});

describe("rotatePointsAroundOrigin", () => {
  it("should rotate 90 degrees counter-clockwise", () => {
    const points: Point[] = [{ x: 1, y: 0 }];
    const rotated = rotatePointsAroundOrigin(points, Math.PI / 2);
    expect(rotated[0].x).toBeCloseTo(0);
    expect(rotated[0].y).toBeCloseTo(1);
  });
});

describe("pointsToEdges", () => {
  it("should return empty for less than 2 points", () => {
    expect(pointsToEdges([])).toEqual([]);
    expect(pointsToEdges([{ x: 0, y: 0 }])).toEqual([]);
  });

  it("should close the polygon loop", () => {
    const edges = pointsToEdges([{ x: 0, y: 0 }, { x: 1, y: 0 }, { x: 1, y: 1 }]);
    expect(edges).toHaveLength(3);
    expect(edges[2].begin).toEqual({ x: 1, y: 1 });
    expect(edges[2].end).toEqual({ x: 0, y: 0 });
  });
});

describe("arrow paths", () => {
  it("should produce non-empty path for a line", () => {
    const path = edgeArrowPath([{ x: 0, y: 0 }, { x: 10, y: 0 }], 2, 1);
    expect(path).toContain("M");
    expect(path).toContain("L");
  });

  it("should close polygon arrows", () => {
    const path = polygonArrowPath([{ x: 0, y: 0 }, { x: 10, y: 0 }, { x: 5, y: 10 }], 2, 1);
    expect(path).toContain("M");
    expect(path.split("M").length - 1).toBe(3);
  });
});
