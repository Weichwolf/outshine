"""Opaque stops and verified single-column receiver ownership, offline only."""

import numpy as np
from urban_parent_conditional import grid
from urban_parent_reference import trace_resolved


def prism_grid_offset(parent_grid, resolved):
    """Match canonical XY footprint/frames without scanning child geometry."""
    pitch = parent_grid["header"]["pitch_m"]
    offset = (resolved["reference_origin_m"] - parent_grid["reference_origin_m"])[
        :2
    ] / pitch
    if (
        not np.isclose(resolved["header"]["pitch_m"], pitch, rtol=0, atol=1e-12)
        or resolved["header"].get("footprint_fraction", 1)
        != parent_grid["header"]["footprint_fraction"]
        or not np.allclose(offset, np.rint(offset), rtol=0, atol=1e-9)
        or resolved["reference_origin_m"][2] != parent_grid["reference_origin_m"][2]
        or not np.array_equal(resolved.get("basis", np.eye(3)), np.eye(3))
    ):
        return None
    return np.rint(offset).astype(np.int64)


def has_camera_miss_witness(parent_grid, resolved, regions):
    """Match a constructor-guaranteed prism field and queried camera domain.

    The geometry tag proves representation form, not domain completeness. The
    caller separately declares complete, actually traced camera regions.
    """
    if (
        resolved is None
        or regions is None
        or not len(regions)
        or resolved.get("column_geometry") != "single_grounded_prism"
        or "fallback" in resolved
    ):
        return False
    offset = prism_grid_offset(parent_grid, resolved)
    if offset is None:
        return False
    low = resolved["low"] + offset
    high = low + resolved["shape"]
    return bool(
        np.all(np.asarray(regions)[:, 0] >= low)
        and np.all(np.asarray(regions)[:, 1] <= high)
    )


def known_receiver_cells(parent_grid, resolved, owners, hits, work=None):
    """Certify only matching, bottom-zero single convex prisms from actual hits."""
    offset = prism_grid_offset(parent_grid, resolved)
    if offset is None:
        return None
    rows = np.flatnonzero(hits & (owners >= 0) & (owners < len(resolved["spans"])))
    if not len(rows):
        return None
    if work is not None:
        work["ownership_column_searches"] = len(rows)
        work["ownership_metadata_logical_B"] = (
            len(rows)
            * 2
            * sum(
                resolved[key].dtype.itemsize for key in ("columns", "spans", "offsets")
            )
        )
    spans = owners[rows].astype(resolved["offsets"].dtype, copy=False)
    columns = np.searchsorted(resolved["offsets"], spans, side="right") - 1
    single = resolved["offsets"][columns + 1] - resolved["offsets"][columns] == 1
    bounds = resolved["spans"][spans]
    cell = resolved["columns"][columns] + offset
    valid = single & (bounds[:, 0] == 0) & (bounds[:, 1] > 0)
    valid &= np.all((cell >= 0) & (cell < parent_grid["shape"]), axis=1)
    if not np.any(valid):
        return None
    result = np.full((len(owners), 2), -1, np.int64)
    result[rows[valid]] = cell[valid]
    return result


def opaque_receivers(
    parent, origins, directions, camera_child, foreground, fraction, *, ownership=False
):
    stop = -origins[:, 2] / directions[:, 2]
    normals = np.tile([0.0, 0.0, 1.0], (len(origins), 1))
    albedo = np.full(len(origins), 0.2)
    work, cells = {}, None
    if camera_child is not None:
        depth, normal, owners, work = trace_resolved(camera_child, origins, directions)
        hits = depth < stop
        stop[hits], normals[hits], albedo[hits] = depth[hits], normal[hits], 0.6
    if foreground:
        plane = (120 - origins[:, 2]) / directions[:, 2]
        points = origins + plane[:, None] * directions
        width = parent.frame[2] * parent.frame[3]
        front = (points[:, 0] < width * fraction) & (plane < stop)
        stop[front], normals[front], albedo[front] = plane[front], [0, 0, 1], 0.35
        if camera_child is not None and ownership:
            hits &= ~front
    if camera_child is not None and ownership:
        hits &= np.einsum("ij,ij->i", normals, directions) < 0
        cells = known_receiver_cells(grid(parent), camera_child, owners, hits, work)
    return stop, normals, albedo, cells, work
