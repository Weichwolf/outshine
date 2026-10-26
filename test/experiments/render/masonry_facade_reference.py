"""Independent screen-pixel clipping with separate facade and brick coordinates."""

import itertools

import numpy as np
from facade_footprint import screen_polygon
from masonry_reference import bricks, periodic_rectangles


def exact(at, dx, dy, fractions, storeys_per_m):
    width = np.abs(dx) + np.abs(dy)
    low, high = at - width / 2, at + width / 2
    brick_rectangles = bricks(low[2:], high[2:])
    brick = [
        screen_polygon(at[2:], dx[2:], dy[2:], r[:2], r[2:]) for r in brick_rectangles
    ]
    windows, bands = [], []
    if min(fractions) > 0:
        rectangles = periodic_rectangles(
            low[:2], high[:2], np.ones(2), (1 - fractions) / 2, (1 + fractions) / 2
        )
        windows = [
            screen_polygon(at[:2], dx[:2], dy[:2], r[:2], r[2:]) for r in rectangles
        ]
        fraction = min(0.16 * max(storeys_per_m, 0), 1 - fractions[1])
        rows = range(int(np.floor(low[1])) - 1, int(np.ceil(high[1])) + 1)
        bands = [
            screen_polygon(
                at[:2],
                dx[:2],
                dy[:2],
                [low[0] - 1, row - fraction / 2],
                [high[0] + 1, row + fraction / 2],
            )
            for row in rows
        ]
    wall = 1 - sum(p.area for p in windows) - 0.35 * sum(p.area for p in bands)
    covered_brick = sum(p.area for p in brick)
    for masks, weight in ((windows, 1), (bands, 0.35)):
        covered_brick -= weight * sum(
            a.intersection(b).area for a, b in itertools.product(brick, masks)
        )
    return 0.84 * wall - covered_brick


def controls():
    # Different coordinate frames: a window fills the pixel, then a plain wall.
    at = np.array([0.5, 0.5, 0.125, 0.04])
    dx = np.array([0.01, 0, 0.0025, 0])
    dy = np.array([0, 0.01, 0, 0.0008])
    assert abs(exact(at, dx, dy, np.array([0.48, 0.53]), 1 / 3.3)) < 1e-12
    assert abs(exact(at, dx, dy, np.zeros(2), 0) + 0.16) < 1e-12
    # A complete 250x160mm repeat has the same mean even if the facade map is singular.
    assert abs(exact(at, [0, 0, 0.25, 0], [0, 0, 0, 0.16], np.zeros(2), 0)) < 1e-12
    return 3
