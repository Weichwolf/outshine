"""Fit existing source-owned prism planes to saved native vertices, offline only."""

from dataclasses import replace

import numpy as np
from attributed_prisms import render_product
from packed_normals import unpack
from primitive_bvh import build
from roof_prism_control import explicit_mesh


def assign_triangles(prisms, source):
    """Propose nearest outward planes; this is not recovered native face identity."""
    groups = [[] for _ in prisms.planes]
    work = {"source_groups": 0, "triangle_plane_pairs": 0, "assigned_triangles": 0}
    for identity in np.unique(prisms.binding[:, 0]):
        owners = np.flatnonzero(prisms.binding[:, 0] == identity)
        planes = np.concatenate(
            [np.arange(prisms.offsets[o], prisms.offsets[o + 1]) for o in owners]
        )
        faces = source["indices"][source["binding"][:, 1] == identity]
        if not len(faces):
            raise ValueError("represented source has no native triangles")
        triangles = source["positions"][faces].astype(float)
        normal = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        length = np.linalg.norm(normal, axis=1)
        if np.any(length == 0) or not np.isfinite(triangles).all():
            raise ValueError("finite nondegenerate native triangles required")
        normal /= length[:, None]
        equations = prisms.planes[planes]
        residual = np.abs(
            np.einsum("tvi,pi->tvp", triangles, equations[:, :3]) - equations[:, 3]
        ).max(1)
        residual = np.where(normal @ equations[:, :3].T > 0, residual, np.inf)
        if np.any(~np.isfinite(residual.min(1))):
            raise ValueError("native triangle has no outward candidate plane")
        chosen = planes[np.argmin(residual, axis=1)]
        for triangle, plane in zip(triangles, chosen, strict=True):
            groups[plane].append(triangle)
        work["source_groups"] += 1
        work["triangle_plane_pairs"] += len(faces) * len(planes)
        work["assigned_triangles"] += len(faces)
    return groups, work


def fitted_planes(prisms, source):
    groups, work = assign_triangles(prisms, source)
    planes = prisms.planes.copy()
    work.update(fitted_planes=0, unchanged_planes=0, unique_fit_points=0)
    old_residual, new_residual = 0.0, 0.0
    for index, group in enumerate(groups):
        if not group:
            work["unchanged_planes"] += 1
            continue
        points = np.unique(np.asarray(group).reshape(-1, 3), axis=0)
        centre = points.mean(0)
        _, singular, vectors = np.linalg.svd(points - centre, full_matrices=False)
        if len(singular) < 3 or singular[1] == 0:
            raise ValueError("assigned plane needs noncollinear native points")
        normal = vectors[-1]
        if normal @ planes[index, :3] < 0:
            normal = -normal
        original = np.abs(points @ planes[index, :3] - planes[index, 3])
        updated = np.abs((points - centre) @ normal)
        old_residual = max(old_residual, float(original.max()))
        new_residual = max(new_residual, float(updated.max()))
        planes[index] = [*normal, normal @ centre]
        work["fitted_planes"] += 1
        work["unique_fit_points"] += len(points)
    work["planes_copy_bytes"] = planes.nbytes
    return planes, work | {
        "original_max_assigned_vertex_residual_m": old_residual,
        "fitted_max_assigned_vertex_residual_m": new_residual,
    }


def fit_product(product, source):
    """Keep ownership/material/fallback records; rebuild fitted bounds before query."""
    prisms, materials, _hierarchy, fallback, prepared = product
    if not len(prisms.binding):
        return product, {
            "scope": "No analytic prisms; complete original fallback unchanged."
        }
    planes, report = fitted_planes(prisms, source)
    bounds = []
    for owner, (begin, end) in enumerate(
        zip(prisms.offsets[:-1], prisms.offsets[1:], strict=True)
    ):
        vertices = explicit_mesh(planes[begin:end], materials.frames[owner, 0]).reshape(
            -1, 3
        )
        bounds.append([vertices.min(0), vertices.max(0)])
    bounds = np.asarray(bounds)
    hierarchy = build(bounds)
    result = replace(prisms, planes=planes, bounds=bounds)
    report["new_bounds_bytes"] = bounds.nbytes
    report["new_hierarchy_bytes"] = hierarchy.bounds.nbytes + hierarchy.children.nbytes
    report["scope"] = (
        "Diagnostic fit, not a containment/topology/material boundary certificate. "
        "Native post-vertex triangles are offline input, not avoided cold expansion. "
        "Unassigned planes stay original; source/part/material/fallback records stay shared."
    )
    return (result, materials, hierarchy, fallback, prepared), report


def render_source_shading(product, normal_words, origins, directions):
    """Use source normals for shading while fitted geometry selects the first face."""
    if normal_words.dtype != np.uint32 or normal_words.shape != (
        len(product[0].planes),
    ):
        raise ValueError("one source-owned Oct16x2 normal word per plane required")
    result = render_product(product, origins, directions)
    analytic = result[4] >= 0
    result[1]["normal"][analytic] = unpack(normal_words[result[5][analytic]])
    result[-1]["source_normal_word_queries"] = int(analytic.sum())
    result[-1]["source_normal_word_read_bytes"] = 4 * int(analytic.sum())
    result[-1]["source_normal_output_write_bytes"] = 24 * int(analytic.sum())
    return result


def geometry_arrays(arrays, fitted, normal_words):
    """Keep the input's table widths and omission of redundant primitive bounds."""
    dtype = arrays["node_children"].dtype
    low, high = np.iinfo(dtype).min, np.iinfo(dtype).max
    children = fitted[2].children
    if children.size and (children.min() < low or children.max() > high):
        raise ValueError("fitted hierarchy escapes the existing integer table width")
    return arrays | {
        "prism_planes": fitted[0].planes,
        "prism_bounds": fitted[0].bounds
        if len(arrays["prism_bounds"])
        else arrays["prism_bounds"],
        "node_bounds": fitted[2].bounds,
        "node_children": children.astype(dtype),
        "fit_normal_words": normal_words,
    }
