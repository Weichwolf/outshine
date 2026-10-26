"""Exact rational convex-footprint counts without enumerating recipe centres.

Closed CCW polygons, including degenerate segments/points. Vertical decomposition
sums upper/lower floors with Euclidean reciprocity. Python integers/Fraction are
an experimental arithmetic reference, not a native performance contract.
"""

import math
from fractions import Fraction
from numbers import Integral


def rational(value):
    if isinstance(value, Integral):
        return Fraction(int(value))
    return value if isinstance(value, Fraction) else Fraction(float(value))


def floor_sum(count, denominator, slope, offset, work):
    """Sum floor((slope*i+offset)/denominator), i=0..count-1, signed inputs."""
    if count < 0 or denominator <= 0:
        raise ValueError("nonnegative count and positive denominator required")
    total = 0
    while True:
        work["euclidean_steps"] = work.get("euclidean_steps", 0) + 1
        work["integer_operand_peak_bits"] = max(
            work.get("integer_operand_peak_bits", 0),
            *(abs(v).bit_length() for v in [count, denominator, slope, offset]),
        )
        quotient, slope = divmod(slope, denominator)
        total += quotient * count * (count - 1) // 2
        quotient, offset = divmod(offset, denominator)
        total += quotient * count
        extent = slope * count + offset
        if extent < denominator:
            return total
        count, offset = divmod(extent, denominator)
        denominator, slope = slope, denominator


def line_floor_sum(first, last, slope, offset, work):
    if last < first:
        return 0
    denominator = math.lcm(slope.denominator, offset.denominator)
    a = slope.numerator * (denominator // slope.denominator)
    b = offset.numerator * (denominator // offset.denominator)
    return floor_sum(last - first + 1, denominator, a, b + a * first, work)


def cross(a, b, p):
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def convex_hull(points):
    points = sorted({tuple(rational(v) for v in point) for point in points})
    if len(points) < 2:
        return points
    chains = []
    for ordered in [points, points[::-1]]:
        chain = []
        for point in ordered:
            while len(chain) >= 2 and cross(chain[-2], chain[-1], point) <= 0:
                chain.pop()
            chain.append(point)
        chains.append(chain[:-1])
    return chains[0] + chains[1]


def swept_square(first, last, half):
    """Minkowski sum in canonical-centre coordinates; no source/child geometry."""
    half = rational(half)
    if half <= 0:
        raise ValueError("positive footprint half-side required")
    return convex_hull(
        (rational(p[0]) + x * half, rational(p[1]) + y * half)
        for p in [first, last]
        for x, y in [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    )


def clip_halfplane(polygon, nx, ny, limit):
    if not polygon:
        return []
    result = []
    previous = polygon[-1]
    before = nx * previous[0] + ny * previous[1] - limit
    for point in polygon:
        after = nx * point[0] + ny * point[1] - limit
        if (before < 0) != (after < 0):
            fraction = before / (before - after)
            result.append(
                tuple(
                    a + fraction * (b - a) for a, b in zip(previous, point, strict=True)
                )
            )
        if after >= 0:
            result.append(point)
        previous, before = point, after
    distinct = []
    for point in result:
        if not distinct or point != distinct[-1]:
            distinct.append(point)
    if len(distinct) > 1 and distinct[0] == distinct[-1]:
        distinct.pop()
    return distinct


def clip_rectangle(polygon, low, high):
    if any(a > b for a, b in zip(low, high, strict=True)):
        return []
    for nx, ny, limit in [
        (1, 0, low[0]),
        (-1, 0, -high[0]),
        (0, 1, low[1]),
        (0, -1, -high[1]),
    ]:
        polygon = clip_halfplane(polygon, nx, ny, rational(limit))
    return polygon


def strict_interior(polygon):
    """Integer-equivalent closed inequalities for the open polygon interior."""
    result = polygon
    for a, b in zip(polygon, polygon[1:] + polygon[:1], strict=True):
        nx, ny = a[1] - b[1], b[0] - a[0]
        limit = nx * a[0] + ny * a[1]
        denominator = math.lcm(nx.denominator, ny.denominator, limit.denominator)
        nx, ny, limit = (int(v * denominator) for v in [nx, ny, limit])
        result = clip_halfplane(result, nx, ny, limit + 1)
    return result


def count_centres(polygon, work):
    """Exact count in a convex CCW rational polygon, boundary included."""
    work["counted_polygons"] = work.get("counted_polygons", 0) + 1
    if not polygon:
        return 0
    left = min(p[0] for p in polygon)
    right = max(p[0] for p in polygon)
    total = max(math.ceil(right) - math.ceil(left), 0)
    for a, b in zip(polygon, polygon[1:] + polygon[:1], strict=True):
        dx = b[0] - a[0]
        if dx == 0:
            continue
        slope = (b[1] - a[1]) / dx
        offset = a[1] - slope * a[0]
        first = math.ceil(min(a[0], b[0]))
        last = math.ceil(max(a[0], b[0])) - 1
        sign = -1 if dx > 0 else 1
        total += line_floor_sum(first, last, sign * slope, sign * offset, work)
    if right.denominator == 1:
        ys = [p[1] for p in polygon if p[0] == right]
        total += max(math.floor(max(ys)) - math.ceil(min(ys)) + 1, 0)
    return total
