"""Bounded hard-surface profile sweeps on selected edges of a mitred XY boundary."""

import math
from dataclasses import dataclass

import numpy as np
import shapely


@dataclass(frozen=True, slots=True)
class BandPlan:
    polygon: object
    profile: tuple
    base_m: float = 0.0
    miter_limit: float = 4.0

    def __post_init__(self):
        p = self.polygon
        if not isinstance(p, shapely.Polygon) or not p.is_valid or p.is_empty:
            raise ValueError("A valid nonempty polygon boundary is required")
        if p.has_z or not np.isfinite(shapely.get_coordinates(p)).all():
            raise ValueError("Finite local XY metres required")
        count = sum(len(r.coords) - 1 for r in (p.exterior, *p.interiors))
        if count > 512:
            raise ValueError("Band boundary exceeds 512 edges")
        if not isinstance(self.profile, tuple) or not 3 <= len(self.profile) <= 32:
            raise ValueError("Profile requires an immutable tuple of 3..32 points")
        if any(not isinstance(v, tuple) or len(v) != 2 for v in self.profile):
            raise ValueError(
                "Immutable (outward depth, height) profile points required"
            )
        profile = np.asarray(self.profile, dtype=np.float64)
        if not np.isfinite(profile).all() or np.any(profile < 0):
            raise ValueError("Finite nonnegative profile metres required")
        if (
            tuple(profile[0]) != (0.0, 0.0)
            or profile[-1, 0] != 0
            or profile[-1, 1] <= 0
            or np.any(profile[1:-1, 0] <= 0)
            or np.any(np.diff(profile[:, 1]) < 0)
        ):
            raise ValueError(
                "One monotone outer profile and closing wall contact required"
            )
        outline = shapely.Polygon(profile)
        if not outline.is_valid or outline.area <= 0 or not outline.exterior.is_ccw:
            raise ValueError("Simple counterclockwise nonzero profile required")
        if np.any(np.linalg.norm(np.diff(profile, axis=0), axis=1) == 0):
            raise ValueError("Duplicate profile points")
        if not math.isfinite(self.base_m) or not 1 <= self.miter_limit <= 16:
            raise ValueError("Finite base and miter limit in [1,16] required")


def boundary(plan):
    """Stable oriented ring order; reject offset topology changes before any sweep."""
    polygon = shapely.orient_polygons(plan.polygon)
    vertices, miters, following = [], [], []
    shifted_rings = []
    maximum = max(p[0] for p in plan.profile)
    for ring in (polygon.exterior, *polygon.interiors):
        points = np.asarray(ring.coords)[:-1]
        delta = np.roll(points, -1, axis=0) - points
        lengths = np.linalg.norm(delta, axis=1)
        if np.any(lengths <= 1e-8):
            raise ValueError("Band boundary has a degenerate edge")
        normals = delta[:, [1, 0]] * [1, -1] / lengths[:, None]
        previous = np.roll(normals, 1, axis=0)
        denominator = 1 + (previous * normals).sum(axis=1)
        if np.any(denominator < 1e-10):
            raise ValueError("Band has opposing edge normals")
        directions = (previous + normals) / denominator[:, None]
        if np.any(np.linalg.norm(directions, axis=1) > plan.miter_limit):
            raise ValueError("Band corner exceeds its miter limit")
        shifted = points + maximum * directions
        if np.any(((np.roll(shifted, -1, axis=0) - shifted) * delta).sum(axis=1) <= 0):
            raise ValueError("Band offset reverses a boundary edge")
        following.extend(len(vertices) + np.roll(np.arange(len(points)), -1))
        vertices.extend(points)
        miters.extend(directions)
        shifted_rings.append(shifted)
    shifted = shapely.Polygon(shifted_rings[0], shifted_rings[1:])
    expected = polygon.buffer(maximum, join_style=2, mitre_limit=plan.miter_limit)
    tolerance = max(1e-9, polygon.area * 1e-10)
    if (
        not shifted.is_valid
        or polygon.difference(shifted).area > tolerance
        or shifted.symmetric_difference(expected).area > tolerance
    ):
        raise ValueError("Band offset changes boundary topology or intersects itself")
    return np.asarray(vertices), np.asarray(miters), np.asarray(following)


def edge_footprints(plan):
    """Conservative XY supports for a caller's neighbor/reservation checks."""
    points, directions, following = boundary(plan)
    maximum = max(p[0] for p in plan.profile)
    moved = points + maximum * directions
    return tuple(
        shapely.Polygon([a, b, c, d])
        for a, b, c, d in zip(
            points, points[following], moved[following], moved, strict=True
        )
    )


def _selected(edges, count):
    if edges is None:
        return np.arange(count)
    if (
        not isinstance(edges, tuple)
        or any(type(e) is not int for e in edges)
        or len(set(edges)) != len(edges)
        or any(e < 0 or e >= count for e in edges)
    ):
        raise ValueError("Unique bounded integer edge identities required")
    return np.asarray(sorted(edges), dtype=int)


def _quads(plan, points, directions, following, edges, method):
    profile = np.asarray(plan.profile)
    nxt = np.roll(np.arange(len(profile)), -1)
    ids = np.stack((edges, following[edges], following[edges], edges), axis=1)
    levels = np.stack(
        (np.arange(len(profile)), np.arange(len(profile)), nxt, nxt), axis=1
    )
    if not len(edges):
        return np.empty((0, 4, 3))
    if method == "batch":
        xy = (
            points[ids][:, None]
            + directions[ids][:, None] * profile[levels, 0][None, ..., None]
        )
    else:
        xy = np.asarray(
            [
                points[i][None] + directions[i][None] * profile[levels, 0][..., None]
                for i in ids
            ]
        )
    z = np.broadcast_to(profile[levels, 1] + plan.base_m, xy.shape[:-1])
    return np.concatenate((xy, z[..., None]), axis=-1).reshape(-1, 4, 3)


def band_surface(plan, edges=None, *, max_triangles=131072, method="batch"):
    """Owned triangle arrays. Missing neighbor edges acquire closed profile caps."""
    if method not in ("batch", "scalar"):
        raise ValueError("Unknown profile expansion method")
    if type(max_triangles) is not int or not 0 <= max_triangles <= 2**31:
        raise ValueError("Bounded nonnegative triangle capacity required")
    points, directions, following = boundary(plan)
    edges = _selected(edges, len(points))
    active = np.zeros(len(points), dtype=bool)
    active[edges] = True
    previous = np.argsort(following)
    starts = edges[~active[previous[edges]]]
    ends = following[edges[~active[following[edges]]]]
    profile = np.asarray(plan.profile)
    triangles = len(edges) * len(profile) * 2 + (len(starts) + len(ends)) * (
        len(profile) - 2
    )
    if triangles > max_triangles:
        raise ValueError("Profile output exceeds triangle capacity")
    quads = _quads(plan, points, directions, following, edges, method)
    faces = (
        np.arange(quads.size // 3)
        .reshape(-1, 4)[:, [[0, 1, 2], [0, 2, 3]]]
        .reshape(-1, 3)
    )
    vertices = quads.reshape(-1, 3)
    edge_ids = np.repeat(edges, len(profile) * 2)
    if len(starts) + len(ends):
        lookup = {tuple(p): i for i, p in enumerate(profile)}
        cap_indices = np.array(
            [
                [
                    lookup[tuple(v)]
                    for v in np.asarray(shapely.orient_polygons(t).exterior.coords)[:3]
                ]
                for t in shapely.get_parts(
                    shapely.constrained_delaunay_triangles(shapely.Polygon(profile))
                )
            ]
        )
        cap_vertices, cap_faces, owners = [], [], []
        for ids, reverse in ((starts, False), (ends, True)):
            for i in ids:
                cap_vertices.append(
                    np.c_[
                        points[i] + profile[:, :1] * directions[i],
                        profile[:, 1] + plan.base_m,
                    ]
                )
                indices = cap_indices[:, ::-1] if reverse else cap_indices
                cap_faces.append(
                    indices + len(vertices) + (len(cap_vertices) - 1) * len(profile)
                )
                owners.extend([previous[i] if reverse else i] * len(indices))
        vertices = np.concatenate((vertices, *cap_vertices))
        faces = np.concatenate((faces, *cap_faces))
        edge_ids = np.r_[edge_ids, owners]
    # Expand hard face normals, including mitred run-end caps.
    xyz = vertices[faces]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    length = np.linalg.norm(normal, axis=1)
    if np.any(length <= 1e-12):
        raise ValueError("Degenerate profile surface")
    normal /= length[:, None]
    reference = np.tile([0.0, 0.0, 1.0], (len(normal), 1))
    reference[abs(normal[:, 2]) > 0.9] = [0.0, 1.0, 0.0]
    tangent = np.cross(reference, normal)
    tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
    up = np.cross(normal, tangent)
    uv = np.stack(
        (np.einsum("ijk,ik->ij", xyz, tangent), np.einsum("ijk,ik->ij", xyz, up)),
        axis=-1,
    )
    result = {
        "vertices": xyz.reshape(-1, 3),
        "faces": np.arange(xyz.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normal, 3, axis=0),
        "uv": uv.reshape(-1, 2),
        "edge_ids": np.asarray(edge_ids, dtype=np.uint32),
    }
    return result
