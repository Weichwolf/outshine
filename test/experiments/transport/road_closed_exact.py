"""Bounded exact triangle intersections for stored affine road display bodies."""

from fractions import Fraction as F

import numpy as np


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross2(a, b):
    return a[0] * b[1] - a[1] * b[0]


def clipped(a, b, drop):
    axes = [i for i in range(3) if i != drop]
    clip = [tuple(p[i] for i in axes) for p in b]
    poly = [tuple(F(p[i]) for i in axes) for p in a]
    if cross2(sub(clip[1], clip[0]), sub(clip[2], clip[0])) < 0:
        clip.reverse()
    for first, last in zip(clip, clip[1:] + clip[:1]):
        output = []
        if not poly:
            break
        for start, end in zip(poly[-1:] + poly[:-1], poly):
            ds = cross2(sub(last, first), sub(start, first))
            de = cross2(sub(last, first), sub(end, first))
            if (ds >= 0) != (de >= 0):
                t = ds / (ds - de)
                output.append(tuple(x + t * (y - x) for x, y in zip(start, end)))
            if de >= 0:
                output.append(end)
        poly = output
    return poly, axes


def segment(tri, dist, axis):
    values = [F(p[axis]) for p, d in zip(tri, dist) if d == 0]
    for i, j in ((0, 1), (1, 2), (2, 0)):
        if (dist[i] < 0 < dist[j]) or (dist[j] < 0 < dist[i]):
            values.append(
                F(tri[i][axis] * dist[j] - tri[j][axis] * dist[i], dist[j] - dist[i])
            )
    return (min(values), max(values)) if values else None


def intersects(a, b, shared):
    na = cross(sub(a[1], a[0]), sub(a[2], a[0]))
    nb = cross(sub(b[1], b[0]), sub(b[2], b[0]))
    da = [dot(nb, sub(p, b[0])) for p in a]
    db = [dot(na, sub(p, a[0])) for p in b]
    if min(da) > 0 or max(da) < 0 or min(db) > 0 or max(db) < 0:
        return False
    line = cross(na, nb)
    if any(line):
        axis = max(range(3), key=lambda i: abs(line[i]))
        sa = segment(a, da, axis)
        sb = segment(b, db, axis)
        if sa is None or sb is None:
            return False
        lo, hi = max(sa[0], sb[0]), min(sa[1], sb[1])
        if lo > hi:
            return False
        if not shared or len(shared) == 3:
            return True
        permitted = [p[axis] for p in shared]
        return lo < min(permitted) or hi > max(permitted)
    if any(da) or any(db):
        return False
    poly, axes = clipped(a, b, max(range(3), key=lambda i: abs(na[i])))
    if not poly:
        return False
    if not shared or len(shared) == 3:
        return True
    permitted = [tuple(F(p[i]) for i in axes) for p in shared]
    if len(shared) == 1:
        return any(p != permitted[0] for p in poly)
    first, last = permitted
    return any(
        cross2(sub(last, first), sub(p, first)) != 0
        or any(
            p[i] < min(first[i], last[i]) or p[i] > max(first[i], last[i])
            for i in range(2)
        )
        for p in poly
    )


def qualify_intersections(vertices, faces, *, max_pairs=65536):
    if type(max_pairs) is not int or not 1 <= max_pairs <= 65536:
        raise ValueError("Bounded exact intersection pair limit required")
    ratios = [[float(v).as_integer_ratio() for v in p] for p in vertices]
    den = max(d for row in ratios for _, d in row)
    points = [tuple(n * (den // d) for n, d in row) for row in ratios]
    triangles = vertices[faces]
    low, high = triangles.min(axis=1), triangles.max(axis=1)
    order = np.argsort(low[:, 0])
    pairs = exact = 0
    for n, i in enumerate(order):
        for j in order[n + 1 :]:
            if low[j, 0] > high[i, 0]:
                break
            pairs += 1
            if pairs > max_pairs:
                raise ValueError("exact_intersection_work_budget")
            if np.any(high[i, 1:] < low[j, 1:]) or np.any(high[j, 1:] < low[i, 1:]):
                continue
            exact += 1
            a = [points[int(k)] for k in faces[i]]
            b = [points[int(k)] for k in faces[j]]
            shared = [
                points[k] for k in set(map(int, faces[i])) & set(map(int, faces[j]))
            ]
            if intersects(a, b, shared):
                raise ValueError(f"non-topological surface intersection: {i} {j}")
    return {
        "broad_pairs": pairs,
        "exact_triangle_pairs": exact,
        "self_intersections": 0,
        "integer_grid_denominator_bits": den.bit_length() - 1,
    }
