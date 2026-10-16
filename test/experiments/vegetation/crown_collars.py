"""Demanded local fork envelopes from the existing saved dominant-axis plans."""

import numpy as np
from colonization import pipe_radii
from curved_fork import CurveBranch, CurvedFork
from fork_surface import blocked_voxel_mesh, voxel_mesh
from limb_paths import dominant_paths
from limb_surface import path_recipe
from spatial_crown_cost import restore


def interval_branch(crown, radii, path, first_m, last_m, anchor_m=None, samples=9):
    points = crown.positions[np.asarray(path)]
    distance = np.r_[0, np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))]
    selected = np.linspace(first_m, last_m, samples)
    if anchor_m is not None:
        selected = np.unique(np.r_[selected, anchor_m])
    parameter = np.interp(selected, distance, np.linspace(0, 1, len(path)))
    spine, profile = path_recipe(crown.positions, radii, path)
    shrink = 0.96 * np.cos(np.pi / 8)
    branch = CurveBranch(spine.evaluate(parameter), profile(parameter) * shrink)
    return branch, {
        "nodes": path,
        "parameter": parameter.tolist(),
        "chord_distance_interval_m": [first_m, last_m],
        "shrink": shrink,
    }


def collar_plan(crown, radii, paths, node, strength=2.0, samples=9):
    parent = next(p for p in paths if node in p[1:])
    children = [p for p in paths if p[0] == node]
    if not children or len(children) > 15:
        raise ValueError(
            "a non-root side fork with at most15 demanded branches required"
        )
    length = lambda p: np.r_[
        0,
        np.cumsum(
            np.linalg.norm(np.diff(crown.positions[np.asarray(p)], axis=0), axis=1)
        ),
    ]
    distances = length(parent)
    anchor = distances[parent.index(node)]
    span = 2.5 * radii[node]
    first, last = anchor - span, anchor + span
    if first <= 0 or last >= distances[-1]:
        raise ValueError("complete local parent interval does not fit")
    branches, plans = [], []
    b, p = interval_branch(crown, radii, parent, first, last, anchor, samples)
    branches.append(b)
    plans.append(p)
    for child in children:
        if span >= length(child)[-1]:
            raise ValueError("complete local child interval does not fit")
        b, p = interval_branch(crown, radii, child, 0, span, None, samples)
        branches.append(b)
        plans.append(p)
    blend = float(strength * radii[node] * max(radii[p[1]] for p in children))
    return CurvedFork(branches, blend), {
        "fork_node": node,
        "blend_m2": blend,
        "span_m": float(span),
        "branches": plans,
    }


def demanded_collars(
    inputs, nodes, cells=48, method="blocked", strength=2.0, samples=9
):
    nodes = tuple(nodes)
    if (
        len(nodes) > 8
        or any(type(n) is not int or n <= 0 for n in nodes)
        or len(set(nodes)) != len(nodes)
    ):
        raise ValueError(
            "zero through eight unique explicit non-root fork IDs required"
        )
    if (
        method not in ("dense", "blocked")
        or type(cells) is not int
        or not 16 <= cells <= 96
    ):
        raise ValueError(
            "declared sampler and bounded16 through96 cell resolution required"
        )
    if (
        not np.isfinite(strength)
        or not 0 < strength <= 2
        or type(samples) is not int
        or not 3 <= samples <= 65
    ):
        raise ValueError("bounded positive blend and source sampling required")
    if not nodes:
        return [], []
    crown = restore(inputs)
    radii = pipe_radii(crown.parents, 0.14)
    paths = dominant_paths(crown.positions, crown.parents, radii)
    forks = {p[0] for p in paths if p[0] > 0}
    if not set(nodes) <= forks:
        raise ValueError("all requested IDs must belong to saved side forks")
    fields = [
        collar_plan(crown, radii, paths, n, strength, samples) for n in sorted(nodes)
    ]
    products, plans = [], []
    for field, plan in fields:
        mesh, work = (blocked_voxel_mesh if method == "blocked" else voxel_mesh)(
            field, cells
        )
        products.append(mesh)
        plans.append(plan | work)
    return products, plans
