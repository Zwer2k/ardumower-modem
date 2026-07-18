from shapely.geometry import Polygon, LineString, MultiPolygon
from shapely.affinity import rotate
from shapely.ops import unary_union
import math

# Koordinatensystem wie im Firmware-Code: Meter
perimeter = Polygon([
    (0, 0), (10, 0), (10, 10), (0, 10), (0, 0)
])

exclusion = Polygon([
    (3, 3), (7, 3), (7, 7), (3, 7), (3, 3)
])

areas = perimeter.difference(exclusion)
print("areas type:", type(areas), "areas:", areas)

width = 0.5
angle = 0  # horizontal lines

# Rotate areas by -angle
if angle == 0:
    rotated_areas = areas
else:
    rotated_areas = rotate(areas, -angle, origin=(0, 0), use_radians=False)

# Bounding box of all rotated areas
minx, miny, maxx, maxy = rotated_areas.bounds
margin = width * 2

# Zigzag in rotated space
zigzag = []
last_x = -(max(abs(minx), abs(maxx)) + margin)
for y in [miny - margin + i * width for i in range(int((maxy + margin - (miny - margin)) / width) + 2)]:
    if y > maxy + margin:
        break
    zigzag.append((last_x, y))
    zigzag.append((-last_x, y))
    last_x = -last_x

zigzag_line = LineString(zigzag)
print("zigzag bounds:", zigzag_line.bounds)

# Clip zigzag to each area (like current code)
segments = []
for area in ([rotated_areas] if isinstance(rotated_areas, Polygon) else list(rotated_areas.geoms)):
    inter = zigzag_line.intersection(area)
    if not inter.is_empty:
        if inter.geom_type == 'MultiLineString':
            for seg in inter.geoms:
                segments.append(seg)
        else:
            segments.append(inter)

print("segments count:", len(segments))
for i, seg in enumerate(segments):
    print(i, seg)

# Rotate back
if angle != 0:
    segments = [rotate(seg, angle, origin=(0,0), use_radians=False) for seg in segments]

# Check if any segment crosses the exclusion
for i, seg in enumerate(segments):
    if seg.crosses(exclusion):
        print(f"Segment {i} crosses exclusion!")
    else:
        print(f"Segment {i} does NOT cross exclusion")
