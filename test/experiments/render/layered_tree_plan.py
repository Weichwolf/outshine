"""Shared spatial field nodes; select metadata before reading any field payload."""

import itertools
from dataclasses import dataclass

import numpy as np


@dataclass
class Plan:
    bounds: np.ndarray
    children: np.ndarray
    resolution: int


def stamp(box, eye, resolution, profile):
    """Projected grid-edge demand; NOT a native surface/opacity error bound."""
    observer = np.asarray(eye, float)
    if observer.shape == (3,):
        observer = np.stack((observer, observer))
    focal = float(profile["focal_px"])
    if (
        observer.shape != (2, 3)
        or not np.isfinite(observer).all()
        or np.any(observer[0] > observer[1])
        or not np.isfinite(focal)
        or focal <= 0
        or resolution <= 0
    ):
        raise ValueError("finite observer and positive sampling profile required")
    gap = np.maximum(np.maximum(box[0] - observer[1], observer[0] - box[1]), 0)
    factor = np.sqrt(
        1
        + (profile["width"] / (2 * focal)) ** 2
        + (profile["height"] / (2 * focal)) ** 2
    )
    edge = float(np.ptp(box, axis=0).max() / resolution)
    depth = np.linalg.norm(gap) / factor
    return (
        np.inf if depth - edge <= 0.1 else float(focal * factor * edge / (depth - edge))
    )


def plan(
    bounds,
    observer,
    profile,
    *,
    resolution=8,
    target_px=1.0,
    max_depth=7,
    max_nodes=16384,
):
    """Cook-time source-bound spatial split; triangles crossing cells are not lost."""
    bounds = np.asarray(bounds, float)
    if (
        bounds.ndim != 3
        or bounds.shape[1:] != (2, 3)
        or not len(bounds)
        or not np.isfinite(bounds).all()
        or np.any(bounds[:, 0] > bounds[:, 1])
        or resolution not in (4, 8, 16, 32)
        or not 0 < target_px <= 4
    ):
        raise ValueError(
            "finite source boxes and bounded explicit field profile required"
        )
    low = bounds[:, 0].min(0)
    high = bounds[:, 1].max(0)
    margin = 128 * np.finfo(float).eps * max(abs(bounds).max(), 1)
    side = 2 ** np.ceil(np.log2(max(np.max(high - low) + 2 * margin, 1e-6)))
    low = low - margin
    nodes, children = [], []
    work = {"source_box_tests": 0, "member_indices": 0}

    def split(box, ids, depth):
        if len(nodes) >= max_nodes:
            raise ValueError("field-tree node budget exhausted")
        node = len(nodes)
        nodes.append(box)
        children.append([-1] * 8)
        work["member_indices"] += len(ids)
        if depth < max_depth and stamp(box, observer, resolution, profile) > target_px:
            half = np.ptp(box, axis=0) / 2
            for child, offset in enumerate(itertools.product((0, 1), repeat=3)):
                a = box[0] + half * np.array(offset)
                b = a + half
                work["source_box_tests"] += len(ids)
                keep = ~((bounds[ids, 1] < a).any(1) | (bounds[ids, 0] > b).any(1))
                if keep.any():
                    children[node][child] = split(
                        np.array([a, b]), ids[keep], depth + 1
                    )
        return node

    split(np.array([low, low + side]), np.arange(len(bounds)), 0)
    return (
        Plan(np.array(nodes), np.array(children, np.int32), resolution),
        work,
    )


def select(tree, eye, profile, *, target_px=1.0):
    selected, unresolved = [], []
    pending = [0]
    tests = 0
    while pending:
        node = pending.pop()
        tests += 1
        if stamp(tree.bounds[node], eye, tree.resolution, profile) <= target_px:
            selected.append(node)
            continue
        children = tree.children[node]
        children = children[children >= 0]
        if len(children):
            pending.extend(map(int, children))
        else:
            unresolved.append(node)
    return np.array(selected, np.int32), np.array(unresolved, np.int32), tests
