"""Exact witnesses for convex rings under an explicitly declared clip/round model.

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


def convex(points):
    points = clean(points)
    if len(points) < 3:
        return False
    turns = [cross(points[i - 1], b, points[(i + 1) % len(points)]) for i, b in enumerate(points)]
    # Consistent turns alone do not exclude star polygons: all vertices must lie
    # in every oriented edge's closed half-plane as well.
    sign = 1 if turns[0] > 0 else -1
    return all(t * sign > 0 for t in turns) and all(
        cross(a, b, p) * sign >= 0
        for a, b in zip(points, points[1:] + points[:1]) for p in points)


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
    return points


def round_coordinate(value):
    value = F(value) + F(1, 2)
    return value.numerator // value.denominator


def rounded(points):
    return clean([tuple(round_coordinate(v) for v in p) for p in points])


def certifies(witness, observations):
    """observations contains (unclosed ring, window) pairs in one shared grid."""
    if not observations or not convex(witness):
        return False
    for expected, window in observations:
        if not convex(expected):
            return False
        actual = rounded(clip(witness, window))
        if not convex(actual) or token(actual) != token(expected):
            return False
    return True


def search(complete, observations, attempts=128, seed=1578):
    """A bounded search; every positive must pass the separate exact certificate."""
    if not convex(complete):
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
