"""Conditional whole-source XY clip exclusion before fine view demand.

This is an explicit change from the old Clip-Unknown Whole contract. It is a
view-only research entry point, not World, shadow or simulation admission.
"""

import numpy as np
import source_raster_candidates as fine
from source_cluster_bounds import (
    EPS32,
    EPS64,
    TINY32,
    clip_boxes,
    clip_envelope,
    frame,
    upper,
)
from source_hierarchy import query as original_query

PLANES = ("left", "right", "bottom", "top")


def outside_xy(bounds, operands):
    """Upper bounds for x+w, w-x, y+w, w-y; strict negative excludes all.

    Bounds must enclose the complete immutable original source. The producer
    separately establishes normal-or-zero source coordinates and static shader
    semantics; frame() establishes the uploaded operand arithmetic contract.
    """
    if (
        bounds.dtype != np.dtype("<f4")
        or bounds.shape != (2, 3)
        or not np.isfinite(bounds).all()
        or np.any(bounds[0] > bounds[1])
    ):
        return None
    with np.errstate(over="ignore", invalid="ignore"):
        envelope = clip_envelope(bounds, operands)
    if envelope is None:
        return None
    lo, hi, error = envelope
    low, high = np.nextafter(lo - error, -np.inf), np.nextafter(hi + error, np.inf)
    upper = np.array(
        [high[0] + high[3], high[3] - low[0], high[1] + high[3], high[3] - low[1]]
    )
    # Account for host evaluation of the final plane sums before strict sign.
    scale = np.max(abs(np.r_[low, high]))
    upper = np.nextafter(upper + 64 * EPS64 * scale, np.inf)
    return upper if np.isfinite(upper).all() else None


def outside_xy_batched(bounds, normal, operands, *, batch_size=256):
    """Conservative XY exclusions for independent whole-source boxes."""
    if (
        bounds.dtype != np.dtype("<f4")
        or bounds.ndim != 3
        or bounds.shape[1:] != (2, 3)
        or np.shape(normal) != (len(bounds),)
        or type(batch_size) is not int
        or not 1 <= batch_size <= 65536
    ):
        raise ValueError("Bounded source box array and normal flags required")
    excluded = np.zeros(len(bounds), dtype=bool)
    if operands is None:
        return excluded
    placement, shift, matrix = operands
    for begin in range(0, len(bounds), batch_size):
        part = bounds[begin : begin + batch_size]
        valid = (
            np.asarray(normal[begin : begin + batch_size], dtype=bool)
            & np.isfinite(part).all(axis=(1, 2))
            & (part[:, 0] <= part[:, 1]).all(axis=1)
        )
        with np.errstate(over="ignore", invalid="ignore"):
            absolute = np.column_stack(
                (np.max(abs(part.astype(float)), axis=1), np.ones(len(part)))
            )
            gamma = 7 * EPS32 / (1 - 7 * EPS32)
            stable = upper(absolute @ abs(placement).T)
            stable_error = upper(gamma * stable + 7 * TINY32)
            world = upper(stable + abs(shift))
            world_error = upper(
                stable_error + EPS32 * upper(world + stable_error) + TINY32
            )
            clip = upper(upper(world + world_error) @ abs(matrix).T)
            error = 2 * upper(
                upper(world_error @ abs(matrix).T) + gamma * clip + 7 * TINY32
            )
            lo, hi = clip_boxes(part, operands)
            low = np.nextafter(lo - error, -np.inf)
            high = np.nextafter(hi + error, np.inf)
            plane_upper = np.column_stack(
                (
                    high[:, 0] + high[:, 3],
                    high[:, 3] - low[:, 0],
                    high[:, 1] + high[:, 3],
                    high[:, 3] - low[:, 1],
                )
            )
            scale = np.max(abs(np.column_stack((low, high))), axis=1)
            plane_upper = np.nextafter(
                plane_upper + (64 * EPS64 * scale)[:, None], np.inf
            )
        with np.errstate(over="ignore", invalid="ignore"):
            maximum = np.finfo(np.float32).max
            valid &= np.isfinite(np.column_stack((lo, hi, error, plane_upper))).all(
                axis=1
            )
            valid &= (stable <= maximum).all(axis=1)
            valid &= (upper(world + world_error) <= maximum).all(axis=1)
            valid &= (clip <= maximum).all(axis=1)
            ambiguous = valid & (abs(plane_upper) <= 1e-10 * scale[:, None]).any(axis=1)
            result = valid & (plane_upper < 0).any(axis=1)
        for index in np.flatnonzero(ambiguous):
            scalar = outside_xy(part[index], operands)
            result[index] = scalar is not None and (scalar < 0).any()
        excluded[begin : begin + len(part)] = result
    return excluded


def query(
    hierarchy, view, placements, raster, *, raster_guard_px, batch_boxes=512, **options
):
    raster = np.asarray(raster)
    fine.validate_query(view, placements, raster, raster_guard_px, **options)
    if type(batch_boxes) is not int or not 1 <= batch_boxes <= 65536:
        raise ValueError("Bounded positive metadata batch required")
    diagnostics = {
        "root_components_tested": 0,
        "plane_upper": None,
        "outside_planes": [],
        "qualified_gpu_clip": False,
        "purpose": "view_only",
    }
    if hierarchy.normal_positions and raster_guard_px < int(raster.max()):
        operands = frame(view, placements)
        if operands is not None:
            diagnostics["root_components_tested"] = 6
            plane_upper = outside_xy(hierarchy.bounds, operands)
            if plane_upper is not None:
                diagnostics["plane_upper"] = plane_upper.tolist()
                diagnostics["outside_planes"] = [
                    PLANES[i] for i in np.flatnonzero(plane_upper < 0)
                ]
    if not diagnostics["outside_planes"]:
        ids, work = original_query(
            hierarchy,
            view,
            placements,
            raster,
            raster_guard_px=raster_guard_px,
            batch_boxes=batch_boxes,
            **options,
        )
        return ids, {**work, "clip": diagnostics}
    ids = np.empty(0, np.uint32)
    return ids, {
        "original_triangles": len(hierarchy.triangles),
        "vertices": len(hierarchy.positions),
        "raster_guard_px": float(raster_guard_px),
        "raster": raster.tolist(),
        "complete_model": True,
        "reason": "source_outside_xy_clip",
        "original_group_retained": False,
        "selected_triangles": 0,
        "output_index_bytes": 0,
        "qualified_gpu_raster": False,
        "bbox_pairs": 0,
        "tested_pairs": 0,
        "accepted_pairs": 0,
        "pair_chunks": 0,
        "max_chunk_pairs": 0,
        "vertex_batch_limit": options.get("vertex_batch_size", fine.VERTEX_BATCH),
        "clip": {
            **diagnostics,
            "fine_position_rows_touched": 0,
            "fine_index_rows_touched": 0,
            "leaf_headers_tested": 0,
            "original_view_triangles_requested": 0,
            "root_bound_bytes": hierarchy.bounds.nbytes,
            "source_payload_still_held_bytes": hierarchy.payload_bytes,
        },
    }
