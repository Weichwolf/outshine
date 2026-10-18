"""Demanded affine composition of saved twig frames and finite Shoot prototypes."""

import numpy as np


def evaluate(base, data, selected, *, max_leaves, anchors=False):
    selected = np.asarray(selected)
    if selected.dtype != bool or selected.shape != base.data["hosts"].shape:
        raise ValueError("one Boolean demand per canonical host required")
    if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
        raise ValueError("explicit bounded leaf capacity required")
    counts = np.diff(base.data["offsets"])
    total = int(counts[selected].sum())
    if total > max_leaves:
        return None
    twigs = np.flatnonzero(
        np.repeat(selected, np.diff(data["host_offsets"]).astype(int))
    )
    starts = np.r_[0, np.cumsum(data["counts"][twigs])]
    result = {
        "ids": np.empty(total, np.uint64),
        "variants": np.empty(total, np.uint8),
        "origins": np.empty((total, 3)),
        "frames": np.empty((total, 3, 3)),
        "twig_ids": np.empty(total, np.uint64),
    }
    if anchors:
        result["nodes"] = np.empty((total, 3))
    for key in np.unique(data["keys"][twigs]):
        chosen = np.flatnonzero(data["keys"][twigs] == key)
        for begin in range(0, len(chosen), 256):
            take = chosen[begin : begin + 256]
            rows = twigs[take]
            n = int(key) // 16
            ordinal = np.arange(n)
            target = starts[take, None] + ordinal
            ids = data["leaf_offsets"][rows, None] + ordinal.astype(np.uint64)
            frame, origin = data["frames"][rows], data["origins"][rows]
            result["ids"][target] = ids
            result["variants"][target] = base.data["variants"][ids]
            result["twig_ids"][target] = data["ids"][rows, None]
            result["origins"][target] = (
                data[f"p{key}_origins"][None] @ frame.swapaxes(-1, -2) + origin[:, None]
            )
            result["frames"][target] = frame[:, None] @ data[f"p{key}_frames"][None]
            if anchors:
                result["nodes"][target] = (
                    data[f"p{key}_nodes"][None] @ frame.swapaxes(-1, -2)
                    + origin[:, None]
                )
    for value in result.values():
        value.setflags(write=False)
    return result
