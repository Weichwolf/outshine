"""Source-index hierarchy and round-the-eye demand cuts before form/mesh work."""

import itertools

import numpy as np
from column_roof_patches import tangent_basis


def source_bounds(plans, cells):
    selected = np.array(
        [i for i, r in enumerate(plans["records"]) if r["cell"] in cells], np.uint32
    )
    if not 0 < len(selected) <= 262144:
        raise ValueError("bounded nonempty source selection required")
    supported = [int(i) for i in selected if plans["summaries"][i]["supported"]]
    if not supported:
        raise ValueError("at least one original source frame required")
    basis = tangent_basis(plans)
    anchor = (
        np.mean([plans["summaries"][i]["frame"][0] for i in supported], axis=0)
        @ basis.T
    )
    corners = np.array(list(itertools.product((0, 1), repeat=3)))
    ids, bounds, needs = [], [], []
    for identity in selected:
        s = plans["summaries"][identity]
        if not s["supported"]:
            needs.append(identity)
            continue
        local = s["bounds"][corners, np.arange(3)]
        points = local @ basis.T - anchor
        if not np.isfinite(points).all():
            needs.append(identity)
            continue
        ids.append(identity)
        bounds.append([points.min(0), points.max(0)])
    return (
        np.array(ids, np.uint32),
        np.array(bounds),
        {
            "basis": basis,
            "anchor": anchor,
            "selected_sources": selected,
            "unbounded_sources": np.array(needs, np.uint32),
        },
    )


def build_tree(identities, bounds):
    ids, bounds = np.asarray(identities, np.uint32), np.asarray(bounds, float)
    if (
        not 0 < len(ids) <= 262144
        or bounds.shape != (len(ids), 2, 3)
        or not np.isfinite(bounds).all()
        or np.any(bounds[:, 0] > bounds[:, 1])
        or len(np.unique(ids)) != len(ids)
    ):
        raise ValueError("unique sources and finite ordered bounds required")
    nodes, children, spans, order = [], [], [], []
    centres = bounds.mean(axis=1)

    def split(indices):
        at = len(nodes)
        low, high = bounds[indices, 0].min(0), bounds[indices, 1].max(0)
        nodes.append([low, high])
        children.append([-1, -1])
        spans.append([len(order), len(indices)])
        if len(indices) == 1:
            order.append(ids[indices[0]])
        else:
            axis = int(np.argmax(np.ptp(centres[indices], axis=0)))
            sorted_ids = indices[np.lexsort((ids[indices], centres[indices, axis]))]
            mid = len(indices) // 2
            children[at] = [split(sorted_ids[:mid]), split(sorted_ids[mid:])]
        return at

    split(np.arange(len(ids)))
    return {
        "node_bounds": np.array(nodes),
        "node_children": np.array(children, np.int32),
        "node_sources": np.array(spans, np.uint32),
        "source_order": np.array(order, np.uint32),
    }


def select_cut(tree, eye, focal_px, target_span_px=8):
    eye = np.asarray(eye, float)
    if (
        eye.shape != (3,)
        or not np.isfinite(eye).all()
        or not np.isfinite([focal_px, target_span_px]).all()
        or min(focal_px, target_span_px) <= 0
    ):
        raise ValueError("finite eye and positive focal/detail scale required")
    stack, selected, visited = [0], [], 0
    while stack:
        at = stack.pop()
        visited += 1
        low, high = tree["node_bounds"][at]
        radius = np.linalg.norm(high - low) / 2
        nearest = np.linalg.norm((high + low) / 2 - eye) - radius
        span = 2 * radius * focal_px / max(nearest, 0.1)
        a, b = tree["node_children"][at]
        if a < 0 or span <= target_span_px:
            selected.append(at)
        else:
            stack.extend((int(b), int(a)))
    return np.array(selected, np.uint32), {
        "visited_nodes": visited,
        "selected_nodes": len(selected),
        "target_estimated_span_px": target_span_px,
        "scope": "Orientation-independent radial size heuristic, not a projected-surface/coverage/PBR error certificate, frustum/occlusion culling or Ready admission.",
    }


def sources_at(tree, node):
    start, count = tree["node_sources"][node]
    return tree["source_order"][start : start + count]
