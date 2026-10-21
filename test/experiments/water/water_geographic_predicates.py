"""Filtered predicates for the stored Float64 water geometry, not real Earth.

Outward scalar intervals assume IEEE nearest binary64 arithmetic. Uncertain
decisions use exact rational arithmetic over the original stored coordinates.
There is no geometric epsilon, triangle enlargement or source-mask snapping.
"""

import math
from dataclasses import dataclass
from fractions import Fraction
from itertools import pairwise

import shapely


@dataclass(frozen=True)
class Interval:
    low: float
    high: float

    @classmethod
    def value(cls, value):
        return cls(float(value), float(value))

    def __add__(self, other):
        return Interval(
            math.nextafter(self.low + other.low, -math.inf),
            math.nextafter(self.high + other.high, math.inf),
        )

    def __sub__(self, other):
        return Interval(
            math.nextafter(self.low - other.high, -math.inf),
            math.nextafter(self.high - other.low, math.inf),
        )

    def __mul__(self, other):
        values = [a * b for a in (self.low, self.high) for b in (other.low, other.high)]
        return Interval(
            math.nextafter(min(values), -math.inf),
            math.nextafter(max(values), math.inf),
        )

    def __truediv__(self, other):
        if other.low <= 0 <= other.high:
            raise ValueError("uncertain interval denominator")
        values = [a / b for a in (self.low, self.high) for b in (other.low, other.high)]
        return Interval(
            math.nextafter(min(values), -math.inf),
            math.nextafter(max(values), math.inf),
        )

    def midpoint(self):
        return self.low / 2 + self.high / 2


def subtract(a, b):
    return tuple(x - y for x, y in zip(a, b, strict=True))


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def coefficients(triangle, origin, direction, cast):
    a, b, c = [tuple(cast(float(x)) for x in p) for p in triangle]
    o, d = [tuple(cast(float(x)) for x in p) for p in (origin, direction)]
    e1, e2, offset = subtract(b, a), subtract(c, a), subtract(o, a)
    p = cross(d, e2)
    determinant = dot(e1, p)
    u = dot(offset, p)
    q = cross(offset, e1)
    v = dot(d, q)
    along = dot(e2, q)
    return determinant, u, v, determinant - u - v, along


def exact_hit(values):
    determinant, u, v, w, _ = values
    # Roots are oriented upward; back-facing antipodal line hits are excluded.
    return determinant < 0 and max(u, v, w) <= 0


def filtered_hit(values):
    determinant, *barycentric, _ = values
    if determinant.low >= 0:
        return False
    if determinant.high >= 0:
        return None
    if any(value.low > 0 for value in barycentric):
        return False
    if all(value.high <= 0 for value in barycentric):
        return True
    return None


def parameters(values, triangle, cast):
    determinant, u, v, _, _ = values
    u, v = u / determinant, v / determinant
    return tuple(
        cast(float(triangle[0, axis]))
        + u * cast(float(triangle[1, axis] - triangle[0, axis]))
        + v * cast(float(triangle[2, axis] - triangle[0, axis]))
        for axis in range(2)
    )


def enclose_fraction(value):
    middle = float(value)
    exact = Fraction(middle)
    return Interval(
        math.nextafter(middle, -math.inf) if exact > value else middle,
        math.nextafter(middle, math.inf) if exact < value else middle,
    )


def above(value, query):
    if value > query.high:
        return True
    if value < query.low:
        return False
    return None


def filtered_ring(ring, point, tick):
    inside = False
    x, y = point
    for a, b in pairwise(ring):
        tick("ring_edge_tests")
        ay, by = above(a[1], y), above(b[1], y)
        if ay is None or by is None:
            return None
        if ay == by:
            continue
        a, b = [tuple(Interval.value(v) for v in p) for p in (a, b)]
        orientation = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])
        if orientation.low <= 0 <= orientation.high:
            return None
        if (orientation.low > 0) == by:
            inside = not inside
    return inside


def exact_ring(ring, point, tick):
    inside = False
    x, y = point
    for first, second in pairwise(ring):
        tick("exact_edge_tests")
        a, b = [tuple(Fraction(v) for v in p) for p in (first, second)]
        orientation = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])
        if (
            orientation == 0
            and min(a[0], b[0]) <= x <= max(a[0], b[0])
            and min(a[1], b[1]) <= y <= max(a[1], b[1])
        ):
            return 0
        if (a[1] > y) != (b[1] > y) and (orientation > 0) == (b[1] > y):
            inside = not inside
    return 1 if inside else -1


def source_members(polygons, point, exact_point, tick, *, exact=False):
    selected = []
    for index, polygon in enumerate(polygons):
        tick("source_bbox_tests")
        xmin, ymin, xmax, ymax = polygon.bounds
        if (
            point[0].high < xmin
            or point[0].low > xmax
            or point[1].high < ymin
            or point[1].low > ymax
        ):
            continue
        covered = False
        for part in shapely.get_parts(polygon):
            rings = [tuple(r.coords) for r in (part.exterior, *part.interiors)]
            locations = (
                None if exact else [filtered_ring(r, point, tick) for r in rings]
            )
            if locations is None or any(value is None for value in locations):
                tick("exact_mask_tests")
                p = exact_point()
                values = [exact_ring(r, p, tick) for r in rings]
                covered |= values[0] == 0 or (
                    values[0] == 1 and all(v != 1 for v in values[1:])
                )
            else:
                covered |= locations[0] and not any(locations[1:])
        if covered:
            selected.append(index)
    return selected
