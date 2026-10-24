"""Exact planar partition for bounded interior, exterior and convex pool contacts."""

from fractions import Fraction

import numpy as np
import shapely


def _cross(a, b, p):
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def _area(poly):
    return sum(a[0] * b[1] - a[1] * b[0] for a, b in zip(poly, poly[1:] + poly[:1]))


def _clip(poly, a, b, sign=1):
    result = []
    for previous, current in zip(poly[-1:] + poly[:-1], poly):
        dp, dc = sign * _cross(a, b, previous), sign * _cross(a, b, current)
        if (dp < 0) != (dc < 0):
            t = dp / (dp - dc)
            result.append(
                tuple(previous[i] + t * (current[i] - previous[i]) for i in (0, 1))
            )
        if dc >= 0:
            result.append(current)
    unique = []
    for p in result:
        if not unique or p != unique[-1]:
            unique.append(p)
    if len(unique) > 1 and unique[0] == unique[-1]:
        unique.pop()
    return unique


def _tri_clip(poly, triangle):
    for a, b in zip(triangle, triangle[1:] + triangle[:1]):
        if not poly:
            break
        poly = _clip(poly, a, b)
    return poly


def _triangulate(pieces, cancelled):
    nodes = {p for poly in pieces for p in poly}
    if len(nodes) > 1024:
        raise ValueError("exact pool-cell node budget exhausted")
    faces, work, determinants = [], 0, 0
    for poly in pieces:
        if cancelled is not None and cancelled():
            raise ValueError("exact pool-cell partition cancelled")
        centre = tuple(sum(p[i] for p in poly) / len(poly) for i in (0, 1))
        boundary = []
        for a, b in zip(poly, poly[1:] + poly[:1]):
            work += len(nodes)
            if work > 262144:
                raise ValueError("exact pool-cell edge-node work budget exhausted")
            axis = 0 if a[0] != b[0] else 1
            other = 1 - axis
            low, high = min(a[axis], b[axis]), max(a[axis], b[axis])
            other_low, other_high = min(a[other], b[other]), max(a[other], b[other])
            candidates = [(Fraction(0), a)]
            for p in nodes:
                if not (low < p[axis] < high and other_low <= p[other] <= other_high):
                    continue
                determinants += 1
                if _cross(a, b, p) == 0:
                    t = (p[axis] - a[axis]) / (b[axis] - a[axis])
                    if 0 < t < 1:
                        candidates.append((t, p))
            boundary.extend(p for _, p in sorted(candidates))
        faces.extend(
            (centre, a, b) for a, b in zip(boundary, boundary[1:] + boundary[:1])
        )
    points = sorted({p for face in faces for p in face})
    ids = {p: i for i, p in enumerate(points)}
    indices = np.array([[ids[p] for p in face] for face in faces], dtype=np.int64)
    xy = np.array([[float(v) for v in p] for p in points])
    if len(np.unique(xy, axis=0)) != len(points):
        raise ValueError("distinct exact nodes collapse during Float64 materialization")
    error = [
        max(abs(Fraction.from_float(float(p[i])) - p[i]) for p in points)
        for i in (0, 1)
    ]
    return xy, indices, points, work, [str(v) for v in error], determinants


def _outside_parts(triangle, edges, interior, area, capacity, cancelled):
    if area == 0:
        return [triangle]
    if area == _area(triangle):
        return []
    outside, remainder = [], triangle
    for a, b in edges:
        if not remainder:
            break
        if cancelled is not None and cancelled():
            raise ValueError("exact pool-cell partition cancelled")
        part = _clip(remainder, a, b, -1)
        if part and _area(part) > 0:
            if len(outside) + len(interior) >= capacity:
                raise ValueError("exact pool-cell piece budget exhausted")
            outside.append(part)
        remainder = _clip(remainder, a, b)
    contained = all(
        _cross(a, b, p) >= 0 for poly in interior for p in poly for a, b in edges
    )
    if not contained or area != (_area(remainder) if remainder else 0):
        raise ValueError("convex edge half-planes do not describe local pool interior")
    return outside


def partition(body, terrain_vertices, terrain_faces, *, cancelled=None):
    """Qualify local convex contacts; preserve the supplied planar domain before rounding."""
    if cancelled is not None and cancelled():
        raise ValueError("exact pool-cell partition cancelled")
    caps = body["contact_vertices"][body["contact_faces"], :2]
    source = np.asarray(terrain_vertices)[:, :2]
    faces = np.asarray(terrain_faces)
    if not 0 < len(faces) <= 64 or len(caps) > 2048 or len(caps) * len(faces) > 32768:
        raise ValueError("bounded exact pool-cell candidate work required")
    if not np.isfinite(source).all() or not np.isfinite(caps).all():
        raise ValueError("finite pool-cell coordinates required")
    source_polygons = shapely.polygons(source[faces])
    if not shapely.coverage_is_valid(source_polygons):
        raise ValueError("nonoverlapping conforming supplied terrain required")
    domain = shapely.coverage_union_all(source_polygons)
    ring = body["plan_xy"][body["outer_ring"]]
    segments = np.stack((ring, np.roll(ring, -1, axis=0)), axis=1)
    selected = np.flatnonzero(shapely.intersects(shapely.linestrings(segments), domain))
    if len(selected) > 64:
        raise ValueError("bounded exact pool-cell outer-edge work required")

    def exact(p):
        return tuple(Fraction.from_float(float(v)) for v in p)

    edges = [tuple(map(exact, segments[i])) for i in selected]
    pieces, pairs = [], 0
    for f in faces:
        if cancelled is not None and cancelled():
            raise ValueError("exact pool-cell partition cancelled")
        triangle = [exact(p) for p in source[f]]
        if _area(triangle) <= 0:
            raise ValueError("positive supplied terrain triangles required")
        interior, area = [], Fraction(0)
        low, high = source[f].min(axis=0), source[f].max(axis=0)
        candidates = np.flatnonzero(
            np.all(caps.max(axis=1) >= low, axis=1)
            & np.all(caps.min(axis=1) <= high, axis=1)
        )
        for index in candidates:
            if cancelled is not None and cancelled():
                raise ValueError("exact pool-cell partition cancelled")
            pairs += 1
            poly = _tri_clip(triangle, [exact(p) for p in caps[index]])
            if poly and _area(poly) > 0:
                if len(pieces) + len(interior) >= 128:
                    raise ValueError("exact pool-cell piece budget exhausted")
                interior.append(poly)
                area += _area(poly)
        outside = _outside_parts(
            triangle, edges, interior, area, 128 - len(pieces), cancelled
        )
        pieces.extend(outside)
        pieces.extend(interior)
        if len(pieces) > 128:
            raise ValueError("exact pool-cell piece budget exhausted")
    xy, indices, points, work, rounding, determinants = _triangulate(pieces, cancelled)
    expected = sum(_area([exact(p) for p in source[f]]) for f in faces)
    if sum(_area(poly) for poly in pieces) != expected:
        raise ValueError("exact pool-cell partition changed the supplied area")
    return (
        xy,
        indices,
        points,
        {
            "input_triangles": len(faces),
            "candidate_pairs": pairs,
            "pool_boundary_edges_considered": len(edges),
            "pieces": len(pieces),
            "edge_node_tests": work,
            "edge_node_determinants": determinants,
            "vertices": len(xy),
            "triangles": len(indices),
            "exact_doubled_area": str(expected),
            "maximum_xy_rounding_fraction_m": rounding,
            "scope": "Exact rational planar partition before Float64 output; no exact geodetic, physical or Float32 claim. Adjacent patch must share original-edge parameters and output words.",
        },
    )
