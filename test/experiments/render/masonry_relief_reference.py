"""Independent clipped-polygon integral of the point bevel's linear gradients."""

import itertools

import numpy as np
from facade_footprint import screen_polygon
from masonry_reference import periodic_rectangles


def pieces(low, high, bevel):
    return (
        (low - bevel / 2, low + bevel / 2, 1 / bevel, 0),
        (low + bevel / 2, high - bevel / 2, 0, 1),
        (high - bevel / 2, high + bevel / 2, -1 / bevel, 1),
    )


def masks(at, dx, dy, fractions, per_m):
    extent = (np.abs(dx) + np.abs(dy)) / 2
    low, high = at - extent, at + extent
    result = []
    if min(fractions) > 0:
        rectangles = periodic_rectangles(
            low, high, np.ones(2), (1 - fractions) / 2, (1 + fractions) / 2
        )
        result.extend((1, screen_polygon(at, dx, dy, r[:2], r[2:])) for r in rectangles)
    stripe = min(0.16 * max(per_m, 0), 1 - fractions[1])
    if stripe > 0:
        for row in range(int(np.floor(low[1])) - 1, int(np.ceil(high[1])) + 1):
            result.append(
                (
                    0.35,
                    screen_polygon(
                        at,
                        dx,
                        dy,
                        [low[0] - 1, row - stripe / 2],
                        [high[0] + 1, row + stripe / 2],
                    ),
                )
            )
    return [(weight, shape) for weight, shape in result if shape.area > 0]


def integral(shape, at, dx, dy, horizontal, vertical, depth):
    if shape.area == 0:
        return np.zeros(2)
    centre = np.array(shape.centroid.coords[0])
    uv = at + centre[0] * dx + centre[1] * dy
    u_value = horizontal[2] * (uv[0] - horizontal[0]) + horizontal[3]
    v_value = vertical[2] * (uv[1] - vertical[0]) + vertical[3]
    return (
        shape.area
        * depth
        * np.array(
            [
                horizontal[2] * v_value,
                vertical[2] * u_value,
            ]
        )
    )


def exact(at, dx, dy, fractions, per_m, depth=0.002, bevel=0.002):
    material_at, material_dx, material_dy = at[2:], dx[2:], dy[2:]
    extent = (np.abs(material_dx) + np.abs(material_dy)) / 2
    low, high = material_at - extent - bevel / 2, material_at + extent + bevel / 2
    excluded = masks(at[:2], dx[:2], dy[:2], fractions, per_m)
    result = np.zeros(2)
    work = {"bricks": 0, "pieces": 0, "masks": len(excluded), "intersections": 0}
    for row in range(
        int(np.floor(low[1] / 0.08)) - 1, int(np.ceil(high[1] / 0.08)) + 1
    ):
        shift = (row % 2) * 0.125
        first = int(np.floor((low[0] - shift) / 0.25)) - 1
        last = int(np.ceil((high[0] - shift) / 0.25)) + 1
        for column in range(first, last + 1):
            x, y = column * 0.25 + shift, row * 0.08
            work["bricks"] += 1
            for u, v in itertools.product(
                pieces(x + 0.005, x + 0.245, bevel),
                pieces(y + 0.005, y + 0.075, bevel),
            ):
                if u[2] == v[2] == 0:
                    continue
                if any(
                    material_dx[axis] == material_dy[axis] == 0
                    and not item[0] <= material_at[axis] < item[1]
                    for axis, item in enumerate((u, v))
                ):
                    continue
                shape = screen_polygon(
                    material_at, material_dx, material_dy, [u[0], v[0]], [u[1], v[1]]
                )
                work["pieces"] += 1
                result += integral(
                    shape, material_at, material_dx, material_dy, u, v, depth
                )
                if shape.area == 0:
                    continue
                for weight, mask in excluded:
                    clipped = shape.intersection(mask)
                    work["intersections"] += 1
                    result -= weight * integral(
                        clipped, material_at, material_dx, material_dy, u, v, depth
                    )
    return result, work
