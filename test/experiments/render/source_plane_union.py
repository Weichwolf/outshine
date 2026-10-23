"""Source-polygon projection and exact planar union, without sampled alpha charts."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.affinity import affine_transform


@dataclass
class PlaneUnion:
    normal: np.ndarray
    offset: float
    axes: np.ndarray
    domain: object
    patches: np.ndarray
    fallback: bool
    original: object | None


def weighted_offset(low, high, weights):
    """Maximum weighted closed-interval overlap for one fixed normal direction."""
    low, high, weights = (np.asarray(a, float) for a in (low, high, weights))
    if (
        low.ndim != 1
        or high.shape != low.shape
        or weights.shape != low.shape
        or not len(low)
        or not np.isfinite(np.r_[low, high, weights]).all()
        or np.any(low > high)
        or np.any(weights <= 0)
    ):
        raise ValueError(
            "nonempty finite ordered intervals and positive weights required"
        )
    events = np.unique(np.r_[low, high])
    starts = np.zeros(len(events))
    ends = np.zeros(len(events))
    np.add.at(starts, np.searchsorted(events, low), weights)
    np.add.at(ends, np.searchsorted(events, high), weights)
    before = np.r_[0, np.cumsum(starts - ends)[:-1]]
    return float(events[np.argmax(before + starts)])


def plane_axes(normal):
    axis = np.eye(3)[np.argmin(abs(normal))]
    first = np.cross(axis, normal)
    first /= np.linalg.norm(first)
    return np.stack((first, np.cross(normal, first)))


def projected_polygon(patch, axes):
    transform = patch.axes @ axes.T
    shift = patch.origin @ axes.T
    return affine_transform(
        patch.domain,
        [transform[0, 0], transform[1, 0], transform[0, 1], transform[1, 1], *shift],
    )


def make_union(patches, ids, normal, offset, fallback):
    axes = plane_axes(normal)
    polygons = [projected_polygon(patches[i], axes) for i in ids]
    domain = shapely.union_all(polygons)
    if not domain.is_valid or domain.is_empty or domain.area <= 0:
        raise ValueError("valid positive-area projected source union required")
    return PlaneUnion(
        normal,
        offset,
        axes,
        domain,
        np.asarray(ids),
        fallback,
        patches[ids[0]] if fallback else None,
    )


def assess_boundaries(groups, vertices, normals, work):
    work["projection_validation_vertex_dots"] = 0
    for group in groups:
        for identity in group.patches:
            values = vertices[identity] @ group.normal - group.offset
            error = float(abs(values).max())
            work["projection_validation_vertex_dots"] += len(values)
            work["maximum_vertex_projection_m"] = max(
                work["maximum_vertex_projection_m"], error
            )
        angle = np.degrees(
            np.arccos(np.clip(normals[group.patches] @ group.normal, -1, 1))
        ).max()
        work["maximum_normal_deviation_degrees"] = max(
            work["maximum_normal_deviation_degrees"], float(angle)
        )


def compile_unions(patches, *, epsilon=3.0, angle_degrees=0.5):
    if (
        not 0 < len(patches) <= 16384
        or not np.isfinite([epsilon, angle_degrees]).all()
        or epsilon <= 0
        or not 0 <= angle_degrees < 1
    ):
        raise ValueError(
            "bounded source patches and positive finite projection profile required"
        )
    vertices = [p.origin + shapely.get_coordinates(p.domain) @ p.axes for p in patches]
    if sum(map(len, vertices)) > 1048576 or not all(
        np.isfinite(p).all() for p in vertices
    ):
        raise ValueError("bounded finite source vertices required")
    normals = np.asarray([p.normal for p in patches])
    if not np.allclose(np.linalg.norm(normals, axis=1), 1, rtol=0, atol=1e-10):
        raise ValueError("unit source normals required")
    area = np.array(
        [p.domain.area * np.linalg.norm(np.cross(*p.axes)) for p in patches]
    )
    if not np.isfinite(area).all() or np.any(area <= 0):
        raise ValueError("positive finite source patch areas required")
    remaining = np.ones(len(patches), bool)
    groups, membership = [], np.full(len(patches), -1, np.int32)
    work = {
        "source_patches": len(patches),
        "normal_family_dot_tests": 0,
        "vertex_plane_dots": 0,
        "weighted_interval_events": 0,
        "input_contour_coordinates": sum(map(len, vertices)),
        "fallback_patches": 0,
        "retained_singleton_patches": 0,
        "normal_families": 0,
        "maximum_vertex_projection_m": 0.0,
        "maximum_normal_deviation_degrees": 0.0,
    }
    cosine = np.cos(np.radians(angle_degrees))
    while remaining.any():
        seed = int(np.argmax(np.where(remaining, area, -np.inf)))
        normal = normals[seed]
        candidates = np.flatnonzero(remaining)
        compatible = normals[candidates] @ normal >= cosine
        ids = candidates[compatible]
        if seed not in ids:
            ids = np.r_[seed, ids]
        remaining[ids] = False
        work["normal_family_dot_tests"] += len(candidates)
        work["normal_families"] += 1
        projections = [vertices[i] @ normal for i in ids]
        work["vertex_plane_dots"] += sum(map(len, projections))
        low = np.array([p.max() - epsilon for p in projections])
        high = np.array([p.min() + epsilon for p in projections])
        pending = low <= high
        for local in np.flatnonzero(~pending):
            identity = int(ids[local])
            patch = patches[identity]
            membership[identity] = len(groups)
            groups.append(
                make_union(
                    patches,
                    [identity],
                    patch.normal,
                    float(patch.origin @ patch.normal),
                    True,
                )
            )
            work["fallback_patches"] += 1
        while pending.any():
            active = np.flatnonzero(pending)
            work["weighted_interval_events"] += 2 * len(active)
            if work["weighted_interval_events"] > 2**26:
                raise ValueError("bounded total weighted interval work required")
            weights = area[ids[active]] * (normals[ids[active]] @ normal)
            offset = weighted_offset(low[active], high[active], weights)
            selected = active[(low[active] <= offset) & (offset <= high[active])]
            if not len(selected):
                raise ValueError("interval selection must consume source patches")
            members = ids[selected]
            membership[members] = len(groups)
            if len(members) == 1:
                patch = patches[int(members[0])]
                group = make_union(
                    patches,
                    members,
                    patch.normal,
                    float(patch.origin @ patch.normal),
                    True,
                )
                work["retained_singleton_patches"] += 1
                work["fallback_patches"] += 1
            else:
                representative = selected[np.argmax(area[members])]
                point = projections[representative]
                offset = float(
                    np.clip(
                        (point.min() + point.max()) / 2,
                        low[selected].max(),
                        high[selected].min(),
                    )
                )
                group = make_union(patches, members, normal, offset, False)
            groups.append(group)
            pending[selected] = False
    if np.any(membership < 0):
        raise ValueError("every patch must remain represented")
    work["plane_unions"] = len(groups)
    work["union_contour_coordinates"] = sum(
        len(shapely.get_coordinates(g.domain)) for g in groups
    )
    assess_boundaries(groups, vertices, normals, work)
    if work["maximum_vertex_projection_m"] > epsilon + 1e-8:
        raise ValueError("actual source boundary projection exceeds declared profile")
    return groups, membership, work
