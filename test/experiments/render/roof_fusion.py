"""Exact convex inner coverage from an accepted roof mesh; no hull infill."""

import itertools
from collections import defaultdict
from fractions import Fraction as F

import numpy as np
from outshine_ref.geometry.occlusion import (
    OpaqueUmbra,
    _bounded_boxes,
    _coefficient_intervals,
    _cross,
    _dot,
    _sub,
)


def empty():
    return OpaqueUmbra(np.empty((0, 4)), np.empty((0, 4)))


def mesh_chart(triangles):
    """Keep original represented vertices and oriented triangle-edge incidences."""
    values = np.asarray(triangles, dtype=np.float64)
    if (
        values.ndim != 3
        or values.shape[1:] != (3, 3)
        or not 1 <= len(values) <= 256
        or not np.isfinite(values).all()
        or np.abs(values).max() > 1e7
    ):
        raise ValueError("bounded finite accepted roof triangles required")
    vertices, indices, lookup = [], [], {}
    for triangle in values:
        ids = []
        for vertex in triangle:
            key = tuple(float(x) for x in vertex)
            if key not in lookup:
                lookup[key] = len(vertices)
                vertices.append(tuple(F(x) for x in key))
            ids.append(lookup[key])
        normal = _cross(
            _sub(vertices[ids[1]], vertices[ids[0]]),
            _sub(vertices[ids[2]], vertices[ids[0]]),
        )
        if any(normal):
            indices.append((ids, normal))
    if not indices:
        raise ValueError("nondegenerate accepted surface required")
    _, normal = max(indices, key=lambda row: sum(x * x for x in row[1]))
    axis = max(range(3), key=lambda index: abs(normal[index]))
    u, v = (axis + 1) % 3, (axis + 2) % 3
    if normal[axis] < 0:
        u, v = v, u
    normal = tuple(x / abs(normal[axis]) for x in normal)
    offset = -_dot(normal, vertices[indices[0][0][0]])
    return vertices, [item[0] for item in indices], (u, v), normal, offset, values


def orient(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def validate_simple_boundary(ring):
    if len(set(ring)) != len(ring):
        raise ValueError("simple projected boundary required")
    edges = list(zip(ring, ring[1:] + ring[:1], strict=True))

    def between(a, b, p):
        return all(min(x, y) <= z <= max(x, y) for x, y, z in zip(a, b, p, strict=True))

    for i, (a, b) in enumerate(edges):
        c = ring[(i + 2) % len(ring)]
        if (
            orient(a, b, c) == 0
            and sum((y - x) * (z - y) for x, y, z in zip(a, b, c, strict=True)) < 0
        ):
            raise ValueError("backtracking projected boundary")
        for j, (c, d) in enumerate(edges):
            if j <= i or j == (i + 1) % len(ring) or i == (j + 1) % len(ring):
                continue
            ab_c, ab_d, cd_a, cd_b = (
                orient(a, b, c),
                orient(a, b, d),
                orient(c, d, a),
                orient(c, d, b),
            )
            if (ab_c * ab_d < 0 and cd_a * cd_b < 0) or any(
                (value == 0 and between(first, last, p))
                for value, first, last, p in (
                    (ab_c, a, b, c),
                    (ab_d, a, b, d),
                    (cd_a, c, d, a),
                    (cd_b, c, d, b),
                )
            ):
                raise ValueError("self-intersecting projected boundary")


def boundary_kernel(points, triangles, axes):
    """Positive triangle chain covers its simple boundary's convex inner kernel."""
    u, v = axes
    edges = defaultdict(list)
    for a, b, c in triangles:
        area = (points[b][u] - points[a][u]) * (points[c][v] - points[a][v]) - (
            points[b][v] - points[a][v]
        ) * (points[c][u] - points[a][u])
        if area <= 0:
            raise ValueError("one consistently oriented height-field chart required")
        for first, last in ((a, b), (b, c), (c, a)):
            edges[min(first, last), max(first, last)].append((first, last))
    boundary = {}
    incoming = set()
    for edge in edges.values():
        if len(edge) == 2 and edge[0] == edge[1][::-1]:
            continue
        if len(edge) != 1:
            raise ValueError("nonmanifold or duplicate triangle edge")
        first, last = edge[0]
        if first in boundary or last in incoming:
            raise ValueError("simple manifold boundary required")
        boundary[first] = last
        incoming.add(last)
    if not 3 <= len(boundary) <= 8 or set(boundary) != incoming:
        raise ValueError("single bounded simple boundary required")
    ring, at = [], min(boundary)
    while at not in ring:
        ring.append(at)
        at = boundary[at]
    if at != ring[0] or len(ring) != len(boundary):
        raise ValueError("holes or disconnected boundaries cannot be filled")
    validate_simple_boundary([(points[index][u], points[index][v]) for index in ring])
    planes = []
    for first, last in zip(ring, ring[1:] + ring[:1], strict=True):
        a, b = points[first], points[last]
        du, dv = b[u] - a[u], b[v] - a[v]
        if du == dv == 0:
            raise ValueError("nondegenerate projected boundary edges required")
        normal = [F(0)] * 3
        normal[u], normal[v] = -dv, du
        offset = -_dot(normal, a)
        planes.append((tuple(normal), offset))
    return planes


def fused_roof_umbra(triangles, observer_box, *, certified_opaque=False):
    """Use an exact height slab above a proven covered convex inner kernel.

    For each observer and both slab levels, the ray section must stay inside
    every projected boundary halfspace. Every target below the slab then meets
    the continuous, consistently oriented roof graph. Tests are affine in eye
    and target, so observer corners and the shared interval AABB query suffice.
    """
    if type(certified_opaque) is not bool:
        raise ValueError("explicit opacity boolean required")
    if not certified_opaque:
        return empty()
    box = _bounded_boxes(np.asarray(observer_box)[None])[0]
    points, indices, axes, normal, offset, values = mesh_chart(triangles)
    boundary = boundary_kernel(points, indices, axes)
    heights = [_dot(normal, point) + offset for point in points]
    lower, upper = min(heights), max(heights)
    eyes = [
        tuple(F(float(value)) for value in eye)
        for eye in set(itertools.product(*zip(box[0], box[1], strict=True)))
    ]
    if any(_dot(normal, eye) + offset <= upper for eye in eyes):
        return empty()
    planes = [(*(-x for x in normal), lower - offset)]
    for eye in eyes:
        height = _dot(normal, eye) + offset
        for edge, constant in boundary:
            h_eye = _dot(edge, eye) + constant
            for level in {lower, upper}:
                coefficients = tuple(
                    -h_eye * n + (height - level) * e
                    for n, e in zip(normal, edge, strict=True)
                )
                intercept = level * h_eye - h_eye * offset + (height - level) * constant
                planes.append((*coefficients, intercept))
    return OpaqueUmbra(
        *_coefficient_intervals(planes),
        observer_box=box,
        patch_bounds=np.array([values.min(axis=(0, 1)), values.max(axis=(0, 1))]),
    )
