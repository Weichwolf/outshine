"""Exact open-lune relative neighbors for closed-vein source influences."""

import numpy as np


def relative_neighbors(nodes, sources, method="sectors"):
    nodes, sources = np.asarray(nodes, dtype=float), np.asarray(sources, dtype=float)
    for points in (nodes, sources):
        if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
            raise ValueError("finite Nx2 physical coordinates required")
    if not 1 <= len(nodes) <= 2048 or len(sources) > 4096:
        raise ValueError("1..2048 vein nodes and at most 4096 sources required")
    if method not in ("dense", "sectors"):
        raise ValueError("dense or sectors relative-neighbor method required")
    if not len(sources):
        return [], {"blocker_pairs": 0, "candidates": 0}
    if method == "dense" and len(sources) * len(nodes) ** 2 > 600_000_000:
        raise ValueError("dense relative-neighbor comparison exceeds pair budget")
    pair2 = None
    if method == "dense":
        pair2 = np.sum((nodes[:, None] - nodes[None]) ** 2, axis=-1)
    results, comparisons, candidate_count = [], 0, 0
    for source in sources:
        offset = nodes - source
        radius2 = np.sum(offset**2, axis=-1)
        if not np.isfinite(radius2).all():
            raise ValueError("squared physical distances must remain finite")
        candidates = np.arange(len(nodes))
        if method == "sectors":
            angle = np.arctan2(offset[:, 1], offset[:, 0])
            sector = np.floor((angle + np.pi) * (4 / np.pi)).astype(int) % 8
            selected = radius2 == 0
            for index in range(8):
                inside = (sector == index) & (radius2 > 0)
                if np.any(inside):
                    selected |= inside & (radius2 == np.min(radius2[inside]))
            candidates = candidates[selected]
        if comparisons + len(candidates) * len(nodes) > 600_000_000:
            raise ValueError("relative-neighbor request exceeds blocker pair budget")
        distances = (
            pair2[candidates]
            if pair2 is not None
            else np.sum((nodes[candidates, None] - nodes[None]) ** 2, axis=-1)
        )
        # Every node can block, including nodes rejected from the candidate set.
        blocked = np.any(
            (radius2[None, :] < radius2[candidates, None])
            & (distances < radius2[candidates, None]),
            axis=1,
        )
        results.append(candidates[~blocked])
        comparisons += len(candidates) * len(nodes)
        candidate_count += len(candidates)
    return results, {"blocker_pairs": comparisons, "candidates": candidate_count}
