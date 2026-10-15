"""Local envelope additions round branch joins without replacing canonical axes."""

import numpy as np
from curve_frame import arc_frame
from curved_fork import CurveBranch, CurvedFork
from fork_surface import blocked_voxel_mesh, voxel_mesh


def arc_chord_error(length, bend, span, pieces):
    if bend == 0:
        return 0.0
    return float(
        2 * length / abs(bend) * np.sin(abs(bend) * span / length / (4 * pieces)) ** 2
    )


def collar_field(plant, path, shrink=0.96, strength=2.0):
    if not path or not 0.5 <= shrink < 1 or not 0 < strength <= 2:
        raise ValueError("child path and bounded local collar parameters required")
    state, parent = plant.axis(path), plant.axis(path[:-1])
    base, tip = plant.radii(path)
    parent_base, parent_tip = plant.radii(path[:-1])
    parameter = state.parent_parameter
    parent_radius = parent_base + parameter * (parent_tip - parent_base)
    span = 2.5 * parent_radius
    first, last = parameter - span / parent.length, parameter + span / parent.length
    child_end = span / state.length
    if first <= 0 or last >= 1 or child_end >= 1:
        raise ValueError(
            "local collar does not fit complete parent/child axis intervals"
        )
    t = np.array([first, parameter, last])
    parent_points = (
        arc_frame(parent.length, parent.bend, 0, t)[0] @ parent.frame.T + parent.origin
    )
    parent_radii = parent_base + t * (parent_tip - parent_base)
    t_child = np.linspace(0.0, child_end, 5)
    child_points = (
        arc_frame(state.length, state.bend, 0, t_child)[0] @ state.frame.T
        + state.origin
    )
    child_radii = base + t_child * (tip - base)
    field = CurvedFork(
        (
            CurveBranch(
                parent_points,
                parent_radii * shrink,
                arc_chord_error(parent.length, parent.bend, 2 * span, 2),
            ),
            CurveBranch(
                child_points,
                child_radii * shrink,
                arc_chord_error(state.length, state.bend, span, 4),
            ),
        ),
        strength * parent_radius * base,
    )
    return field, {
        "path": path,
        "span_m": float(span),
        "shrink": shrink,
        "strength": strength,
        "source_chord_bounds_m": [b.source_error_bound_m for b in field.branches],
        "parent_interval": [float(first), float(last)],
        "child_interval": [0.0, float(child_end)],
    }


def collar_products(plant, cells=48, method="blocked"):
    if (
        method not in ("dense", "blocked")
        or type(cells) is not int
        or not 16 <= cells <= 64
    ):
        raise ValueError("declared sampler and 16 through 64 local cells required")
    if not 1 <= sum(plant.axis_counts) <= 128:
        raise ValueError("at most 128 axes in a complete collar request required")
    paths = tuple(plant.paths())
    meshes, work = [], []
    for path in paths[1:]:
        field, row = collar_field(plant, path)
        mesh, measured = (voxel_mesh if method == "dense" else blocked_voxel_mesh)(
            field, cells
        )
        meshes.append(mesh)
        work.append(row | measured)
    return meshes, work
