"""Local old terrain planes retained under complete immutable pier feet."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_module_sections import interpolate

GUARD_M = 0.01
MAX_PIECES = 4096


def indexed_reference(triangles):
    raw = np.asarray(triangles, float)
    if (
        raw.ndim != 3
        or raw.shape[1:] != (3, 3)
        or len(raw) > MAX_PIECES
        or not np.isfinite(raw).all()
    ):
        raise ValueError("bounded finite protected terrain triangles required")
    raw = raw.reshape(-1, 3)
    xy, first, inverse = np.unique(
        raw[:, :2], axis=0, return_index=True, return_inverse=True
    )
    heights = raw[first, 2]
    if np.max(abs(heights[inverse] - raw[:, 2]), initial=0) > 1e-7:
        raise ValueError("protected terrain planes disagree at a shared vertex")
    return Mesh(np.c_[xy, heights], inverse.reshape(-1, 3))


def foot_windows(state, region):
    contacts = state["pier_contacts"]
    if len(contacts) > 128:
        raise ValueError("whole-foot terrain protection pier cap")
    windows = []
    for low, _high in contacts:
        if not 0 < len(low.triangles) <= 4096:
            raise ValueError("whole-foot terrain protection cap size")
        original = low.vertices[low.triangles, :2]
        foot = shapely.union_all(shapely.polygons(original))
        if not foot.intersects(region):
            continue
        actual = shapely.union_all(
            shapely.polygons(original.astype(np.float32).astype(float))
        )
        window = shapely.buffer(
            shapely.minimum_rotated_rectangle(shapely.union_all([foot, actual])),
            GUARD_M,
            quad_segs=1,
            join_style="mitre",
        )
        if not window.covers(foot) or not window.covers(actual):
            raise ValueError("protected window lost a whole actual pier foot")
        windows.append(window)
    return windows


def clipped_planes(old, old_xyz, chosen, shapes, windows):
    ids = np.flatnonzero(chosen & ~old["ground"])
    cuts = shapely.intersection(shapes[ids], shapely.union_all(windows))
    parts, parents = shapely.get_parts(cuts, return_index=True)
    keep = (shapely.get_type_id(parts) == 3) & (shapely.area(parts) > 0)
    parts, parents = parts[keep], parents[keep]
    bound = shapely.get_num_coordinates(parts) + 2 * shapely.get_num_interior_rings(
        parts
    )
    if bound.sum() > MAX_PIECES:
        raise ValueError("protected terrain triangulation bound exceeded")
    pieces, owners = shapely.get_parts(
        shapely.constrained_delaunay_triangles(parts), return_index=True
    )
    if len(pieces) > MAX_PIECES:
        raise ValueError("protected terrain piece cap")
    xy = shapely.get_coordinates(pieces).reshape(-1, 4, 2)[:, :3]
    original = old_xyz[old["faces"][ids[parents[owners]]]]
    heights = interpolate(
        np.repeat(original, 3, axis=0),
        xy.reshape(-1, 2),
        np.repeat(original[:, :, 2], 3, axis=0),
    ).reshape(-1, 3)
    triangles = np.concatenate([xy, heights[:, :, None]], axis=2)
    return triangles, {
        "nonroad_parent_candidates": len(ids),
        "intersecting_parent_faces": len(np.unique(parents)),
        "clipped_polygon_coordinates": int(shapely.get_num_coordinates(parts).sum()),
        "triangle_bound": int(bound.sum()),
        "protected_pieces": len(pieces),
        "protected_area_m2": float(shapely.area(shapely.union_all(pieces))),
    }


def fixed(state, chosen, protected_ids, shapes, region, old_xyz, recipe):
    old = state["plan"]
    ground_ids = np.flatnonzero(chosen & old["ground"])
    policy = recipe.get("pier_ground_policy", "whole_parent")
    if policy == "whole_parent":
        triangles = old_xyz[old["faces"][protected_ids]]
        return (
            triangles,
            [Mesh(old_xyz, old["faces"][np.r_[ground_ids, protected_ids]])],
            {},
        )
    if policy != "whole_foot":
        raise ValueError("explicit known pier ground policy required")
    windows = foot_windows(state, region)
    triangles, note = clipped_planes(old, old_xyz, chosen, shapes, windows)
    reference = indexed_reference(triangles)
    note.update(
        policy=policy,
        whole_feet=len(windows),
        geometric_guard_m=GUARD_M,
        protected_reference_vertices=len(reference.vertices),
    )
    return (
        triangles,
        [Mesh(old_xyz, old["faces"][ground_ids]), reference],
        note,
    )
