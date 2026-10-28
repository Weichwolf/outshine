"""Exact witnesses for simple rings under an explicitly declared clip/round model.

This does not model the provider's topology-repair branches or establish identity.
An absent bounded-search witness is unresolved, not a proof of infeasibility.
"""
from fractions import Fraction as F
import random


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])


def clean(points):
    result = []
    for p in points:
        p = tuple(p)
        if not result or p != result[-1]:
            result.append(p)
    if len(result) > 1 and result[0] == result[-1]:
        result.pop()
    changed = True
    while changed and len(result) >= 3:
        changed = False
        for i, b in enumerate(result):
            a, c = result[i - 1], result[(i + 1) % len(result)]
            if cross(a, b, c) == 0 and all(min(a[k], c[k]) <= b[k] <= max(a[k], c[k]) for k in (0, 1)):
                result.pop(i)
                changed = True
                break
    return result


def twice_area(points):
    return sum(a[0] * b[1] - a[1] * b[0] for a, b in zip(points, points[1:] + points[:1]))


def intersects(a, b, c, d):
    def contains(p, q, r):
        return all(min(p[k], q[k]) <= r[k] <= max(p[k], q[k]) for k in (0, 1))
    ab_c, ab_d, cd_a, cd_b = cross(a, b, c), cross(a, b, d), cross(c, d, a), cross(c, d, b)
    if ((ab_c == 0 and contains(a, b, c)) or (ab_d == 0 and contains(a, b, d))
            or (cd_a == 0 and contains(c, d, a)) or (cd_b == 0 and contains(c, d, b))):
        return True
    return ab_c * ab_d < 0 and cd_a * cd_b < 0


def simple(points):
    points = clean(points)
    if len(points) < 3 or twice_area(points) == 0:
        return False
    count = len(points)
    if any(cross(points[i - 1], p, points[(i + 1) % count]) == 0 for i, p in enumerate(points)):
        return False
    edges = list(zip(points, points[1:] + points[:1]))
    for i, (a, b) in enumerate(edges):
        for j in range(i + 1, count):
            if j == i + 1 or i == 0 and j == count - 1:
                continue
            if intersects(a, b, *edges[j]):
                return False
    return True


def token(points):
    points = clean(points)
    if not points:
        return ()
    choices = []
    for order in (points, points[::-1]):
        choices.extend(tuple(order[i:] + order[:i]) for i in range(len(order)))
    return min(choices)


def clip(points, window):
    points = clean(points)
    for axis, limit, lower in [(0, window[0], True), (0, window[2], False),
                               (1, window[1], True), (1, window[3], False)]:
        result = []
        if not points:
            break
        def inside(p):
            return p[axis] >= limit if lower else p[axis] <= limit
        for a, b in zip(points[-1:] + points[:-1], points):
            ia, ib = inside(a), inside(b)
            if ia != ib:
                ratio = F(limit - a[axis]) / (b[axis] - a[axis])
                point = tuple(a[k] + ratio * (b[k] - a[k]) for k in (0, 1))
                result.append(point)
            if ib:
                result.append(b)
        points = clean(result)
        if len(points) < 3 or twice_area(points) == 0:
            return []
        if not simple(points):
            return None
    return points


def round_coordinate(value):
    value = F(value) + F(1, 2)
    return value.numerator // value.denominator


def rounded(points):
    return clean([tuple(round_coordinate(v) for v in p) for p in points])


def certifies(witness, observations):
    """observations contains (unclosed ring, window) pairs in one shared grid."""
    if not observations or not simple(witness):
        return False
    for expected, window in observations:
        if not simple(expected):
            return False
        clipped = clip(witness, window)
        if clipped is None:
            return False
        actual = rounded(clipped)
        if not simple(actual) or token(actual) != token(expected):
            return False
    return True


def search(complete, observations, attempts=128, seed=1578):
    """A bounded search; every positive must pass the separate exact certificate."""
    if not simple(complete):
        return None, 0
    witness = [tuple(F(x) for x in p) for p in complete]
    if certifies(witness, observations):
        return witness, 1
    generator = random.Random(seed)
    for attempt in range(attempts):
        witness = [tuple(F(x) + F(generator.randrange(-3, 4), 8) for x in p) for p in complete]
        if certifies(witness, observations):
            return witness, attempt + 2
    return None, attempts + 1
