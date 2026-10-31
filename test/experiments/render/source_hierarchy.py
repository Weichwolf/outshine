"""Owned static source groups before the existing fine primitive selector."""

from dataclasses import dataclass

import numpy as np
import source_raster_candidates as fine
from source_cluster_bounds import clip_boxes, frame, possible_leaves, support_margin
from source_triangle_clusters import TriangleGroups, immutable
from source_triangle_clusters import build as group_triangles


@dataclass(frozen=True)
class SourceHierarchy:
    positions: np.ndarray
    triangles: np.ndarray
    groups: TriangleGroups
    bounds: np.ndarray
    witnesses: np.ndarray
    normal_positions: bool

    @property
    def source(self):
        return {"positions": self.positions, "indices": self.triangles}

    @property
    def payload_bytes(self):
        return (
            self.positions.nbytes
            + self.triangles.nbytes
            + self.groups.payload_bytes
            + self.bounds.nbytes
            + self.witnesses.nbytes
        )


def prepare(source, *, leaf_triangles=4, order="source"):
    groups = group_triangles(source, leaf_triangles=leaf_triangles, order=order)
    p, t = source["positions"], source["indices"]
    normal = not np.any((abs(p) > 0) & (abs(p) < np.finfo(np.float32).tiny))
    return SourceHierarchy(
        immutable(p, p.dtype),
        immutable(t, t.dtype),
        groups,
        immutable([p.min(0), p.max(0)], "<f4"),
        immutable(p[np.unique(np.r_[p.argmin(0), p.argmax(0)])], "<f4"),
        normal,
    )


def pages(rows, stride, count):
    """Logical 4096-byte pages of one joined numeric array, not physical IO."""
    if rows is None:
        return (count * stride + 4095) // 4096
    if not len(rows):
        return 0
    return len(
        np.unique(np.r_[rows * stride // 4096, (rows * stride + stride - 1) // 4096])
    )


def query(
    hierarchy,
    view_words,
    placements,
    raster,
    *,
    raster_guard_px,
    batch_boxes=512,
    **options,
):
    if type(batch_boxes) is not int or not 1 <= batch_boxes <= 65536:
        raise ValueError("Bounded positive metadata batch required")
    p, t, groups = hierarchy.positions, hierarchy.triangles, hierarchy.groups
    raster = np.asarray(raster)
    work = {
        "headers_tested": 0,
        "headers_kept": len(groups.bounds),
        "metadata_batch_limit": batch_boxes,
        "metadata_bytes_held": groups.payload_bytes
        + hierarchy.bounds.nbytes
        + hierarchy.witnesses.nbytes,
        "witness_points_tested": 0,
        "full_geometry_bytes_held": p.nbytes + t.nbytes,
        "source_vertices": len(p),
        "source_triangles": len(t),
        "bypass": None,
        "coarse_margin_px": None,
    }

    def finish(ids, costs, used=None, faces=None, copied=0):
        return ids, {
            **costs,
            "original_triangles": len(t),
            "vertices": len(p),
            "hierarchy": {
                **work,
                "fine_vertex_rows_touched": len(p) if used is None else len(used),
                "fine_triangle_rows_touched": len(t) if faces is None else len(faces),
                "logical_position_pages_4096": pages(used, 12, len(p)),
                "logical_index_pages_4096": pages(faces, t.dtype.itemsize * 3, len(t)),
                "explicit_gather_copy_bytes": copied,
            },
        }

    def flat(reason):
        work["bypass"] = reason
        return finish(
            *fine.select(
                hierarchy.source,
                view_words,
                placements,
                raster,
                raster_guard_px=raster_guard_px,
                **options,
            )
        )

    def no_fine(complete, reason):
        ids = np.empty(0, np.uint32) if complete else np.arange(len(t), dtype=np.uint32)
        costs = {
            "raster_guard_px": raster_guard_px,
            "raster": raster.tolist(),
            "bbox_pairs": 0,
            "tested_pairs": 0,
            "accepted_pairs": 0,
            "pair_chunks": 0,
            "max_chunk_pairs": 0,
            "qualified_gpu_raster": False,
            "vertex_batch_limit": options.get("vertex_batch_size", fine.VERTEX_BATCH),
            "complete_model": complete,
            "reason": reason,
            "original_group_retained": not complete,
            "selected_triangles": len(ids),
            "output_index_bytes": len(ids) * 12,
        }
        empty = np.empty(0, np.uint32)
        return finish(ids, costs, empty, empty)

    # Invalid inputs use the original validator/error path, without touching a
    # speculative coarse model. Static geometry was validated and frozen once.
    if (
        not isinstance(view_words, np.ndarray)
        or view_words.dtype != np.dtype("<u4")
        or view_words.shape != (56,)
        or not isinstance(placements, np.ndarray)
        or placements.dtype != np.dtype("<u4")
        or placements.shape != (1, 36)
        or raster.shape != (2,)
        or raster.dtype.kind not in "iu"
        or np.any(raster <= 0)
        or np.any(raster > 65536)
        or type(raster_guard_px) not in (int, float)
        or not np.isfinite(raster_guard_px)
        or raster_guard_px < 0
    ):
        return flat("frame_layout")
    limits = {
        "pairs_most": (0, fine.MAX_PAIRS),
        "chunk_pairs": (1, fine.MAX_CHUNK),
        "vertex_batch_size": (1, fine.MAX_VERTICES),
    }
    if any(
        key not in limits
        or type(value) is not int
        or not limits[key][0] <= value <= limits[key][1]
        for key, value in options.items()
    ):
        return flat("query_limits")
    if raster_guard_px >= int(raster.max()):
        work["bypass"] = "guard_whole"
        return no_fine(False, "raster_guard_covers_viewport")
    if not hierarchy.normal_positions:
        work["bypass"] = "unsupported_source"
        return no_fine(False, "clip_or_arithmetic_unknown")
    operands = frame(view_words, placements)
    if operands is None:
        work["bypass"] = "upload_arithmetic"
        return no_fine(False, "clip_or_arithmetic_unknown")
    witnesses = np.repeat(hierarchy.witnesses[:, None, :], 2, axis=1)
    lo, hi = clip_boxes(witnesses, operands)
    work["witness_points_tested"] = len(witnesses)
    outside = (
        (hi[:, 3] <= 0)
        | (hi[:, 2] <= 0)
        | (lo[:, 2] >= hi[:, 3])
        | (lo[:, :2] >= hi[:, 3, None]).any(1)
        | (hi[:, :2] <= -hi[:, 3, None]).any(1)
    )
    if outside.any():
        work["bypass"] = "actual_vertex_outside"
        return no_fine(False, "clip_or_arithmetic_unknown")
    margin = support_margin(hierarchy.bounds, operands, raster.astype(float))
    if margin is None:
        return flat("root_projection")
    work["coarse_margin_px"] = margin.tolist()
    keep = possible_leaves(
        groups.bounds,
        operands,
        raster,
        margin,
        raster_guard_px,
        batch_boxes=batch_boxes,
    )
    work["headers_tested"], work["headers_kept"] = len(keep), int(keep.sum())
    if keep.all():
        return flat("all_headers")
    if int(np.diff(groups.offsets)[keep].sum()) * 4 > len(t) * 3:
        return flat("dense_triangle_subset")
    faces = groups.faces[np.repeat(keep, np.diff(groups.offsets))]
    if not len(faces):
        return no_fine(True, None)
    triangles = t[faces]
    used, local = np.unique(triangles, return_inverse=True)
    positions = p[used]
    selected, costs = fine.select(
        {"positions": positions, "indices": local.reshape(-1, 3)},
        view_words,
        placements,
        raster,
        raster_guard_px=raster_guard_px,
        **options,
    )
    # A local cooperative limit must still return the whole ORIGINAL group.
    ids = (
        faces[selected]
        if costs["complete_model"]
        else np.arange(len(t), dtype=np.uint32)
    )
    ids.sort()
    costs = {
        **costs,
        "selected_triangles": len(ids),
        "output_index_bytes": len(ids) * 12,
        "original_group_retained": len(ids) == len(t),
    }
    copied = faces.nbytes + triangles.nbytes + positions.nbytes
    if costs["complete_model"]:
        copied += ids.nbytes
    return finish(ids, costs, used, faces, copied)
