"""Local spatial-parent collar envelopes from the same stable wood axes."""

import numpy as np
from curved_fork import CurveBranch, CurvedFork
from fork_surface import blocked_voxel_mesh, voxel_mesh
from spatial_axis import SpatialAxis


def chord_bound(axis, parameters):
    if isinstance(axis.shape, SpatialAxis):
        raw = axis.shape.measure.parameter(parameters)
        controls = np.asarray(axis.shape.spine.controls)
        acceleration = 6 * len(controls) ** 2 * np.diff(controls, n=2, axis=1)
        upper = np.linalg.norm(acceleration, axis=-1).max() * axis.pose.scale
        return float(upper * np.diff(raw).max() ** 2 / 8)
    if axis.pose.bend == 0:
        return 0.0
    span = np.diff(parameters).max()
    return float(
        2
        * axis.pose.length
        / abs(axis.pose.bend)
        * np.sin(abs(axis.pose.bend) * span / 4) ** 2
    )


def collar_field(parent, child):
    chart, _, _ = parent.chart(child.pose.anchor[None])
    parameter = float(chart[0, 2] / parent.pose.length)
    radius = float(parent.radius(parameter, inscribed=True))
    span = 2.5 * radius
    first, last = (
        max(0.0, parameter - span / parent.pose.length),
        min(1.0, parameter + span / parent.pose.length),
    )
    end = min(1.0, span / child.pose.length)
    parent_t = np.unique(np.r_[np.linspace(first, last, 9), parameter])
    child_t = np.linspace(0, end, 9)
    branches = tuple(
        CurveBranch(
            axis.centres(t), axis.radius(t, inscribed=True) * 0.96, chord_bound(axis, t)
        )
        for axis, t in ((parent, parent_t), (child, child_t))
    )
    field = CurvedFork(branches, 2 * radius * float(child.radius(0, inscribed=True)))
    return field, {
        "path": child.path,
        "span_m": span,
        "parent_interval": [first, last],
        "child_interval": [0.0, end],
        "source_chord_bounds_m": [b.source_error_bound_m for b in branches],
    }


def graph_collars(axes, cells=48, method="blocked"):
    if (
        method not in ("dense", "blocked")
        or type(cells) is not int
        or not 16 <= cells <= 64
    ):
        raise ValueError("declared sampler and bounded local collar cells required")
    lookup = {a.path: a for a in axes}
    meshes, plans = [], []
    for child in axes:
        parent = lookup.get(child.path[:-1])
        if parent is None:
            continue
        field, plan = collar_field(parent, child)
        mesh, work = (blocked_voxel_mesh if method == "blocked" else voxel_mesh)(
            field, cells
        )
        meshes.append(mesh)
        plans.append(plan | work)
    return meshes, plans
