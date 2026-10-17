"""Nested whole-building mass proposals from actual source boundaries and holes."""

import numpy as np
import shapely
from shapely.affinity import affine_transform

from building_layout import frame
from stepped_form_profiles import outlines


def proposals(polygon, kind, seed):
    if kind not in ("cascade", "paired-wings"):
        raise ValueError("Known terraced mass programme required")
    forward, reverse = frame(polygon)
    local = affine_transform(polygon, forward)
    x0, y0, x1, y1 = local.bounds
    sizes = (x1 - x0, y1 - y0)
    order = (seed % 2, 1 - seed % 2)
    for axis in order:
        if kind == "cascade":
            direction = np.asarray(forward[:2] if axis == 0 else forward[2:4])
            other = np.array([-direction[1], direction[0]])
            distance = min(3.0, sizes[axis] * 0.13)
            if distance < 1.2:
                continue
            for side in (-1, 1) if seed % 4 < 2 else (1, -1):
                a, b = outlines(
                    {"footprint": polygon}, direction, side, (distance, 2 * distance)
                )
                c = outlines({"footprint": polygon}, other, -side, (distance,))[0]
                yield (polygon, a, b.intersection(c)), (axis, side, distance)
            continue
        swap = [0, 1, 1, 0, 0, 0]
        current = local if axis == 0 else affine_transform(local, swap)
        a, b, c, d = current.bounds
        width, depth = c - a, d - b
        if width < 14 or depth < 9:
            continue
        wing = min(depth * 0.30, 5.0)
        for fraction in (0.5, 0.35, 0.65):
            centre = a + width * fraction
            bridge = shapely.box(centre - 3.0, b - 1, centre + 3.0, d + 1)
            first = shapely.box(a - 1, b - 1, c + 1, b + wing)
            second = shapely.box(a - 1, d - wing, c + 1, d + 1)
            middle = current.intersection(shapely.union_all([first, second, bridge]))
            high = current.intersection((first if seed % 2 else second).union(bridge))
            values = (current, middle, high)
            if axis:
                values = tuple(affine_transform(p, swap) for p in values)
            values = tuple(
                affine_transform(p, reverse).intersection(polygon) for p in values
            )
            yield (polygon, *values[1:]), (axis, fraction, wing)
