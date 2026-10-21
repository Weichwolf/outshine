"""Compile conservative extents for the existing closed terraced-window recipe."""

import numpy as np
from terraced_facade_prepare import positions
from window_extent_query import MAX_BOUNDS, MAX_COORDINATE_M, WindowBounds


def compile_bounds(prepared):
    """Existing no-legacy-trim recipe only; no template, floor, mesh or CSG work."""
    if prepared.mass.kind not in ("cascade", "paired-wings"):
        raise ValueError("Unsupported closed-window recipe; retain the coarse product")
    windows = prepared.facade.windows
    if len(windows) > MAX_BOUNDS:
        raise ValueError("Bounded existing window plan required")
    if not windows:
        return WindowBounds((), np.empty((0, 2, 3)))
    center = np.array([w.centre for w in windows], float)
    tangent = np.array([w.tangent for w in windows], float)
    dims = np.array([[w.width_m, w.height_m, w.bottom_m] for w in windows], float)
    core = prepared.mass.core
    frame = np.array([core.origin_xy, core.across_xy, core.along_xy], float)
    if (
        center.shape != (len(windows), 2)
        or tangent.shape != center.shape
        or frame.shape != (3, 2)
        or not all(np.isfinite(v).all() for v in (center, tangent, dims, frame))
        or max(abs(v).max() for v in (center, dims, frame)) > MAX_COORDINATE_M / 4
        or np.any(abs(np.linalg.norm(tangent, axis=1) - 1) > 1e-8)
        or np.any(dims[:, :2] <= 0.11)
    ):
        raise ValueError("Finite existing window dimensions/frame required")
    # Opening-module casing .055 plus sill half-width extension .025 = .08.
    # Sill front -.16, frame back .118; pane/mullion lie inside those reaches.
    # Bottom -.10 includes the sill's -.055-.032-(.16-.045)/12 extent.
    local = np.array(
        [[x, y, z] for x in (-1, 1) for y in (-0.16, 0.118) for z in (0, 1)],
        float,
    )
    along = local[None, :, 0] * (dims[:, :1] / 2 + 0.08)
    inward = local[None, :, 1]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    xy = (
        center[:, None]
        + along[:, :, None] * tangent[:, None]
        + inward[:, :, None] * normal[:, None]
    )
    z = dims[:, None, 2] + np.where(
        local[None, :, 2] == 0, -0.10, dims[:, None, 1] + 0.055
    )
    world = positions(np.dstack((xy, z)).reshape(-1, 3), core).reshape(-1, 8, 3)
    # Finite-recipe arithmetic guard, not a native/global floating-point proof.
    magnitude = (abs(center).sum(1) + abs(dims).sum(1) + 1) * (
        abs(frame[1:]).sum() + 1
    ) + abs(frame[0]).sum()
    guard = 128 * np.finfo(float).eps * magnitude[:, None]
    boxes = np.stack((world.min(1) - guard, world.max(1) + guard), axis=1)
    boxes[:, 0] = np.nextafter(boxes[:, 0], -np.inf)
    boxes[:, 1] = np.nextafter(boxes[:, 1], np.inf)
    return WindowBounds(tuple(w.identifier for w in windows), boxes)
