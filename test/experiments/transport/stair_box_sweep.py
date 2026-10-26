"""Exact triangle hits of an explicitly sized box translated along fixed local curves."""

from fractions import Fraction
from itertools import pairwise

import numpy as np
from outshine_ref.contracts import Mesh

_AXES = ((1, 0, 0), (0, 1, 0), (0, 0, 1))


def rational(point):
    return tuple(Fraction(float(value)) for value in point)


def _sub(a, b):
    return tuple(x - y for x, y in zip(a, b, strict=True))


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _triangle(vertices):
    edges = tuple(_sub(b, a) for a, b in pairwise((*vertices, vertices[0])))
    normal = _cross(edges[0], edges[1])
    if not any(normal):
        raise ValueError("nondegenerate exact obstacle triangles required")
    low = tuple(min(p[i] for p in vertices) for i in range(3))
    high = tuple(max(p[i] for p in vertices) for i in range(3))
    return vertices, edges, normal, low, high


def _sweep(start, end, lower, upper):
    delta = _sub(end, start)
    low = tuple(min(a, b) + c for a, b, c in zip(start, end, lower, strict=True))
    high = tuple(max(a, b) + c for a, b, c in zip(start, end, upper, strict=True))
    normals = tuple(_cross(delta, axis) for axis in _AXES)
    return start, end, lower, upper, delta, low, high, normals


def _remaining_axes(edges, normal, delta, normals):
    yield from normals
    yield normal
    for edge in edges:
        for axis in (*_AXES, delta):
            yield _cross(edge, axis)


def _relation(triangle, sweep):
    vertices, edges, normal, tri_low, tri_high = triangle
    start, end, lower, upper, delta, low, high, normals = sweep
    contact, tested = False, 0
    for i in range(3):
        tested += 1
        if tri_low[i] > high[i] or tri_high[i] < low[i]:
            return "separated", tested
        contact |= tri_low[i] == high[i] or tri_high[i] == low[i]
    for axis in _remaining_axes(edges, normal, delta, normals):
        if not any(axis):
            continue
        tested += 1
        positions = (_dot(start, axis), _dot(end, axis))
        a = min(positions) + sum(
            min(x * lo, x * hi) for x, lo, hi in zip(axis, lower, upper, strict=True)
        )
        b = max(positions) + sum(
            max(x * lo, x * hi) for x, lo, hi in zip(axis, lower, upper, strict=True)
        )
        projections = tuple(_dot(vertex, axis) for vertex in vertices)
        c, d = min(projections), max(projections)
        if c > b or d < a:
            return "separated", tested
        contact |= c == b or d == a
    return "contact" if contact else "interior", tested


def _inputs(mesh, routes, lower, upper, method, max_pairs):
    if (
        not isinstance(mesh, Mesh)
        or not 1 <= len(mesh.triangles) <= 4096
        or not 1 <= len(mesh.vertices) <= 16384
    ):
        raise ValueError("owned nonempty bounded obstacle Mesh required")
    if method not in ("scan", "bounds"):
        raise ValueError("known surface-sweep candidate method required")
    if type(max_pairs) is not int or not 0 <= max_pairs <= 1048576:
        raise ValueError("bounded nonnegative exact-pair budget required")
    if not 1 <= len(routes) <= 64 or sum(map(len, routes)) > 1024:
        raise ValueError("bounded nonempty fixed curve batch required")
    paths = [np.asarray(route, dtype=float) for route in routes]
    lower, upper = np.asarray(lower, float), np.asarray(upper, float)
    arrays = [mesh.vertices, *paths]
    if any(
        a.ndim != 2
        or a.shape[1] != 3
        or not len(a)
        or not np.isfinite(a).all()
        or abs(a).max() > 1e7
        for a in arrays
    ):
        raise ValueError("finite bounded XYZ geometry required")
    if (
        lower.shape != (3,)
        or upper.shape != (3,)
        or not np.isfinite([lower, upper]).all()
        or max(abs(lower).max(), abs(upper).max()) > 5
        or not (lower < upper).all()
    ):
        raise ValueError(
            "explicit positive box dimensions bounded to five metres required"
        )
    segments = sum(max(1, len(path) - 1) for path in paths)
    if method == "scan" and segments * len(mesh.triangles) > max_pairs:
        raise ValueError("exact-pair budget exhausted before surface queries")
    return paths, lower, upper, segments


def sweep_surfaces(mesh, routes, lower, upper, *, method="bounds", max_pairs=262144):
    """Classify contact/interior intersection with selected triangle surfaces.

    The body has fixed caller-selected axes; only translation occurs. The convex
    hull of the two endpoint boxes is the continuous sweep for each segment.
    SAT uses box/sweep face normals, the triangle normal, and edge cross products.
    Fraction arithmetic classifies the supplied binary64 values without an epsilon.
    These are surface hits, not solid containment or a complete locomotion test.
    No source geometry, path or physical policy is changed; execution is off-frame.
    """
    paths, lower, upper, segments = _inputs(
        mesh, routes, lower, upper, method, max_pairs
    )
    vertices, triangles = {}, {}
    low, high = rational(lower), rational(upper)
    bounds_bytes = 0
    if method == "bounds":
        coordinates = mesh.vertices[mesh.triangles]
        minima, maxima = coordinates.min(axis=1), coordinates.max(axis=1)
        bounds_bytes = coordinates.nbytes + minima.nbytes + maxima.nbytes
        del coordinates
    events = []
    work = {
        "input_faces": len(mesh.triangles),
        "segments": segments,
        "potential_pairs": segments * len(mesh.triangles),
        "candidate_pairs": 0,
        "tested_axes": 0,
        "bound_coordinate_bytes": bounds_bytes,
        "selected_index_bytes": 0,
    }
    for route_id, path in enumerate(paths):
        points = [rational(point) for point in path]
        for segment, (start, end) in enumerate(
            pairwise(points if len(points) > 1 else points * 2)
        ):
            sweep = _sweep(start, end, low, high)
            if method == "bounds":
                outward_low = np.nextafter(
                    np.array([float(x) for x in sweep[5]]), -np.inf
                )
                outward_high = np.nextafter(
                    np.array([float(x) for x in sweep[6]]), np.inf
                )
                selected = np.flatnonzero(
                    (minima <= outward_high).all(axis=1)
                    & (maxima >= outward_low).all(axis=1)
                )
                work["selected_index_bytes"] += selected.nbytes
            else:
                selected = range(len(mesh.triangles))
            if work["candidate_pairs"] + len(selected) > max_pairs:
                raise ValueError("exact-pair budget exhausted; no partial result")
            work["candidate_pairs"] += len(selected)
            for face in selected:
                if face not in triangles:
                    for vertex in mesh.triangles[face]:
                        if vertex not in vertices:
                            vertices[vertex] = rational(mesh.vertices[vertex])
                    triangles[face] = _triangle(
                        tuple(vertices[i] for i in mesh.triangles[face])
                    )
                relation, tested = _relation(triangles[face], sweep)
                work["tested_axes"] += tested
                if relation != "separated":
                    events.append([route_id, segment, int(face), relation])
    work.update(exact_vertices=len(vertices), exact_triangles=len(triangles))
    return events, work
