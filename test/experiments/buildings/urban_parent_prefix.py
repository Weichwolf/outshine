"""Atomic categorical prefix intervals; no per-recipe-cell traversal.

Rational footprint counts are exact. Bilinear-density ranges enclose each stored
coarse interpolation block; directed floating log/exp rounding has not been proved.
These are research probability intervals, never native Empty/occluder certificates.
No shading, first-hit depth, opaque-stop composition or readiness is provided.
"""

import math
from fractions import Fraction as F
from itertools import pairwise

import numpy as np
from column_field_query import walk_cells
from lattice_footprint import (
    clip_rectangle,
    count_centres,
    strict_interior,
    swept_square,
)
from urban_parent_conditional import density, events, grid


def dda_prefix(parent, origins, directions, stops=None, *, product=None):
    product = grid(parent) if product is None else product
    work = {"steps": 0, "cell_ray_visits": 0, "height_hypothesis_tests": 0}
    local = origins - product["reference_origin_m"]
    transmission = np.ones(len(origins))
    for ids, cells, begin, leave, keep in walk_cells(
        product, local, directions, work, maximum=stops
    ):
        probability = np.zeros(len(ids))
        for valid, _, _, mass in events(
            parent, product, local, directions, ids, cells, begin, leave
        ):
            probability += valid * mass
            work["height_hypothesis_tests"] += len(ids)
        transmission[ids] *= 1 - probability
        keep[:] = transmission[ids] > 0
    return transmission, work


def prefix_footprint(product, origin, direction, stop, height, *, minimum=0.1):
    """Clip a finite mathematical prefix through a height slab, in cell units."""
    o = [F(v) for v in origin]
    d = [F(v) for v in direction]
    pitch = F(product["header"]["pitch_m"])
    reference = [F(v) for v in product["reference_origin_m"]]
    begin, end = F(minimum), F(stop) if math.isfinite(stop) else None
    bounds = [
        (reference[axis], reference[axis] + int(product["shape"][axis]) * pitch)
        for axis in range(2)
    ] + [(F(0), F(height))]
    for axis, (low, high) in enumerate(bounds):
        if d[axis] == 0:
            if o[axis] < low or o[axis] > high:
                return []
            continue
        near, far = sorted(((low - o[axis]) / d[axis], (high - o[axis]) / d[axis]))
        begin = max(begin, near)
        end = far if end is None else min(end, far)
    if end is None or begin >= end:
        return []
    endpoints = [
        tuple((o[a] + t * d[a] - reference[a]) / pitch - F(1, 2) for a in range(2))
        for t in [begin, end]
    ]
    return swept_square(*endpoints, F(product["header"]["footprint_fraction"]) / 2)


def interpolation_blocks(parent, product):
    """Disjoint inclusive integer rectangles, split at bilinear knots."""
    extent = F(float(parent.frame[2] * parent.frame[3]))
    pitch = F(product["header"]["pitch_m"])
    axes = []
    for axis, count in enumerate(parent.centre_fraction().shape[::-1]):
        reference = F(product["reference_origin_m"][axis])
        knots = [
            math.ceil(((F(j) + F(1, 2)) * extent / count - reference) / pitch - F(1, 2))
            for j in range(-1, count + 1)
        ]
        cuts = sorted(
            {
                0,
                int(product["shape"][axis]),
                *(max(0, min(int(product["shape"][axis]), v)) for v in knots),
            }
        )
        axes.append(list(pairwise(cuts)))
    rectangles = []
    for x0, x1 in axes[0]:
        for y0, y1 in axes[1]:
            if x0 < x1 and y0 < y1:
                rectangles.append(((x0, y0), (x1 - 1, y1 - 1)))
    regions = np.asarray(rectangles, np.int64)
    corners = np.stack(
        [
            regions[:, 0],
            regions[:, 1],
            np.c_[regions[:, 0, 0], regions[:, 1, 1]],
            np.c_[regions[:, 1, 0], regions[:, 0, 1]],
        ],
        axis=1,
    )
    q = density(parent, product, corners.reshape(-1, 2)).reshape(-1, 4)
    guard = 64 * np.finfo(float).eps
    ranges = np.c_[
        np.maximum(q.min(axis=1) - guard, 0),
        np.minimum(q.max(axis=1) + guard, 1 - parent.frame[6]),
    ]
    return regions, ranges


class AtomicPrefix:
    """Bounded coarse data; all canonical recipe centres remain implicit."""

    def __init__(self, parent):
        self.parent = parent
        self.product = grid(parent)
        self.regions, self.ranges = interpolation_blocks(parent, self.product)
        tail = np.cumsum(parent.probabilities[::-1])[::-1]
        void = 1 - self.ranges[..., None] * tail
        above = np.concatenate((void[..., 1:], np.ones_like(void[..., :1])), axis=-1)
        self.weights = np.log(above) - np.log(void)
        self.work = {
            "coarse_blocks": len(self.regions),
            "retained_array_bytes": self.regions.nbytes
            + self.ranges.nbytes
            + self.weights.nbytes,
            "density_corner_samples": 4 * len(self.regions),
            "prefix_queries": 0,
            "height_footprints": 0,
            "block_polygon_pairs": 0,
            "counted_polygons": 0,
            "euclidean_steps": 0,
            "integer_operand_peak_bits": 0,
        }

    def interval(self, origin, direction, stop=np.inf):
        if (
            np.shape(origin) != (3,)
            or np.shape(direction) != (3,)
            or not np.isfinite(origin).all()
            or not np.isfinite(direction).all()
            or np.linalg.norm(direction) == 0
            or origin[2] < 0
            or np.isnan(stop)
            or stop < 0.1
        ):
            raise ValueError("finite above-base prefix geometry required")
        self.work["prefix_queries"] += 1
        depth_low, depth_high = 0.0, 0.0
        for level, height in enumerate(self.parent.heights):
            polygon = prefix_footprint(self.product, origin, direction, stop, height)
            self.work["height_footprints"] += 1
            if not polygon:
                continue
            inner = strict_interior(polygon)
            low = np.array([math.ceil(min(p[a] for p in polygon)) for a in range(2)])
            high = np.array([math.floor(max(p[a] for p in polygon)) for a in range(2)])
            selected = np.flatnonzero(
                ((self.regions[:, 1] >= low) & (self.regions[:, 0] <= high)).all(axis=1)
            )
            for at in selected:
                a, b = self.regions[at]
                outer_count = count_centres(clip_rectangle(polygon, a, b), self.work)
                inner_count = count_centres(clip_rectangle(inner, a, b), self.work)
                depth_low += self.weights[at, 0, level] * inner_count
                depth_high += self.weights[at, 1, level] * outer_count
            self.work["block_polygon_pairs"] += len(selected)
        return np.array([math.exp(-depth_high), math.exp(-depth_low)])
