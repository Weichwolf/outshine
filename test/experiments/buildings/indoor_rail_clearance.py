"""Conservative bounded segment clipping against framed traffic prisms."""

from itertools import pairwise

import numpy as np
import shapely


def merged_intervals(intervals):
    result = []
    for low, high in sorted(intervals):
        low, high = max(0.0, float(low)), min(1.0, float(high))
        if low >= high:
            continue
        if result and low <= result[-1][1]:
            result[-1] = result[-1][0], max(high, result[-1][1])
        else:
            result.append((low, high))
    return result


class RailClearance:
    def __init__(
        self,
        reservations,
        radius_bound_m,
        margin_m,
        *,
        max_reservations=1024,
        max_tests=65536,
    ):
        if (
            type(max_reservations) is not int
            or not 1 <= max_reservations <= 4096
            or type(max_tests) is not int
            or not 1 <= max_tests <= 1048576
            or len(reservations) > max_reservations
            or not np.isfinite((radius_bound_m, margin_m)).all()
            or radius_bound_m <= 0
            or margin_m <= 0
        ):
            raise ValueError(
                "Bounded reservations and positive physical clearance required"
            )
        self.reservations = tuple(reservations)
        self.radius = radius_bound_m + margin_m
        # GEOS rounds with inscribed arc chords. Inflation makes every chord
        # conservative for the requested Euclidean radius, including corners.
        inflated = self.radius / np.cos(np.pi / 64)
        self.radius_xy = inflated
        self.shapes = tuple(
            r.geometry.buffer(inflated, quad_segs=16) for r in reservations
        )
        self.tree = shapely.STRtree(self.shapes)
        self.max_tests, self.tests = max_tests, 0

    def blocked(self, first, last):
        first, last = np.asarray(first, dtype=float), np.asarray(last, dtype=float)
        if (
            first.shape != (3,)
            or last.shape != (3,)
            or not np.isfinite((first, last)).all()
        ):
            raise ValueError("Finite XYZ rail segment required")
        delta = last - first
        horizontal = float(delta[:2] @ delta[:2])
        shape = (
            shapely.LineString((first[:2], last[:2]))
            if horizontal
            else shapely.Point(first[:2])
        )
        candidates = self.tree.query(shape, predicate="intersects")
        self.tests += len(candidates)
        if self.tests > self.max_tests:
            raise ValueError("rail_clearance_candidate_budget")
        intervals = []
        for index in map(int, candidates):
            reservation, polygon = self.reservations[index], self.shapes[index]
            low = reservation.heights.lower_m - self.radius
            high = reservation.heights.upper_m + self.radius
            if delta[2] == 0:
                if not low < first[2] < high:
                    continue
                zrange = (0.0, 1.0)
            else:
                zrange = sorted(
                    ((low - first[2]) / delta[2], (high - first[2]) / delta[2])
                )
            if zrange[1] <= 0 or zrange[0] >= 1:
                continue
            if horizontal == 0:
                if polygon.contains(shape):
                    intervals.append(zrange)
                continue
            if shape.touches(polygon):
                continue
            overlap = shape.intersection(polygon)
            for piece in shapely.get_parts(overlap):
                if piece.geom_type != "LineString":
                    continue
                coordinates = shapely.get_coordinates(piece)
                at = (coordinates - first[:2]) @ delta[:2] / horizontal
                intervals.append(
                    (max(float(at.min()), zrange[0]), min(float(at.max()), zrange[1]))
                )
        return merged_intervals(intervals)

    def clip(self, positions):
        positions = np.asarray(positions, dtype=float)
        result, current = [], []
        for first, last in pairwise(positions):
            blocked = self.blocked(first, last)
            starts, stops = (
                [0.0, *[b for _, b in blocked]],
                [*[a for a, _ in blocked], 1.0],
            )
            for low, high in zip(starts, stops, strict=True):
                if low >= high:
                    continue
                a, b = first + (last - first) * low, first + (last - first) * high
                if current and np.array_equal(current[-1], a):
                    current.append(b)
                else:
                    if current:
                        result.append(np.asarray(current))
                    current = [a, b]
            if blocked and blocked[-1][1] == 1 and current:
                result.append(np.asarray(current))
                current = []
        if current:
            result.append(np.asarray(current))
        return tuple(result)
