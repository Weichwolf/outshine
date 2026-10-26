"""Adaptive exact-candidate field: uncertain leaves never become opaque samples."""

import heapq

import numpy as np
from beam_intervals import Beams
from beam_patch_fusion import Patches


def classify(beams, patches, ids, patch_ids, box, work):
    work["face_beam_tests"] += len(ids)
    possible, full, low, high = beams.classify(ids, box)
    work["covering_references"] += int(full.sum())
    work["outside_references"] += int((~possible).sum())
    threshold = high[full].min(initial=np.inf)
    joint = False
    if len(patch_ids):
        work["patch_beam_tests"] += len(patch_ids)
        patch_possible, patch_full = patches.classify(patch_ids, box)
        work["covering_patch_references"] += int(patch_full.sum())
        threshold = min(
            threshold, patches.high[patch_ids[patch_full]].min(initial=np.inf)
        )
        patch_ids, patch_full = patch_ids[patch_possible], patch_full[patch_possible]
    keep = possible & (low <= threshold)
    work["depth_pruned_references"] += int((possible & ~keep).sum())
    ids, full = ids[keep], full[keep]
    certain = len(ids) == 1 and bool(full[0])
    if patches and len(ids) == 2 and len(patch_ids):
        joint = bool(
            np.any(
                np.all(
                    np.sort(patches.pairs[patch_ids[patch_full]], axis=1)
                    == np.sort(ids),
                    axis=1,
                )
            )
        )
    return ids, patch_ids, 1 if certain else 2 if joint else 0


def build(
    source,
    observer_box,
    profile,
    *,
    max_nodes=8192,
    max_depth=12,
    fuse=True,
    certified_opaque=False,
):
    if certified_opaque is not True:
        raise ValueError("explicit opaque input contract required")
    if not 1 <= max_nodes <= 65536 or not 0 <= max_depth <= 20:
        raise ValueError("bounded explicit node/depth budgets required")
    focal = float(profile["focal_px"])
    if (
        not np.isfinite(focal)
        or focal <= 0
        or min(profile["width"], profile["height"]) <= 0
    ):
        raise ValueError("positive finite projection profile required")
    beams = Beams(source["positions"], source["indices"], observer_box)
    patches = Patches(source, beams) if fuse else None
    work = {
        k: 0
        for k in (
            "face_beam_tests",
            "outside_references",
            "depth_pruned_references",
            "covering_references",
            "patch_beam_tests",
            "covering_patch_references",
        )
    }
    factor = (
        1
        + (profile["width"] / (2 * focal)) ** 2
        + (profile["height"] / (2 * focal)) ** 2
    )
    if beams.depth[0].min() <= 0.1 * np.sqrt(factor):
        raise ValueError("directional product crosses the profile near-clip guard")
    children, states, frontier = [], [], []

    def node(ids, patch_ids, box, depth):
        ids, patch_ids, status = classify(beams, patches, ids, patch_ids, box, work)
        at = len(children)
        children.append([-1] * 4)
        states.append((ids, patch_ids, box, depth, status))
        demand = np.ptp(box, axis=0).max() * focal * factor
        if len(ids) and not status and depth < max_depth and demand > 0.5:
            # Spend the shared budget on the largest remaining query workload.
            priority = float(np.prod(np.ptp(box, axis=0))) * len(ids)
            heapq.heappush(frontier, (-priority, at))
        return at

    node(
        np.arange(len(source["indices"])),
        np.arange(len(patches.pairs)) if patches else np.empty(0, int),
        beams.bounds,
        0,
    )
    while frontier and len(children) + 4 <= max_nodes:
        _, at = heapq.heappop(frontier)
        ids, patch_ids, box, depth, _ = states[at]
        middle = box.mean(0)
        for child in range(4):
            offset = np.array([child & 1, (child >> 1) & 1])
            a, b = np.where(offset, middle, box[0]), np.where(offset, box[1], middle)
            children[at][child] = node(ids, patch_ids, np.array([a, b]), depth + 1)
        states[at] = None
    ranges, references, certified = [], [], []
    for state in states:
        ids, status = (state[0], state[4]) if state else (np.empty(0, int), 0)
        ranges.append([len(references), len(ids)])
        references.extend(map(int, ids))
        certified.append(status)
    arrays = {
        "children": np.asarray(children, np.int32),
        "ranges": np.asarray(ranges, np.uint32),
        "references": np.asarray(references, np.uint32),
        "certified": np.asarray(certified, np.uint8),
        "basis": beams.basis,
        "bounds": beams.bounds,
    }
    return arrays, work | {
        "prepared_interval_bytes": beams.prepared_array_bytes,
        "nodes": len(children),
        "program_references": len(references),
        "unique_faces": len(np.unique(references)),
        "opaque_patch_pairs": len(patches.pairs) if patches else 0,
        "certified_single_leaves": certified.count(1),
        "certified_joint_leaves": certified.count(2),
        "mixed_leaves": sum(
            bool(r[1]) and c == 0 for r, c in zip(ranges, certified, strict=True)
        ),
        "budget_leaves": len(frontier),
    }
