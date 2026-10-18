"""Deterministic dominant paths retain every source edge and its canonical node IDs."""

import numpy as np
from form_plan import validate_parents


def dominant_paths(positions, parents, radii, max_nodes=4096, *, continuations=None):
    positions = np.asarray(positions, np.float64)
    parents = np.asarray(parents)
    radii = np.asarray(radii, np.float64)
    if type(max_nodes) is not int or not 2 <= max_nodes <= 65536:
        raise ValueError("explicit bounded skeleton capacity required")
    validate_parents(parents)
    if len(parents) > max_nodes:
        return None
    if (
        len(parents) < 2
        or positions.shape != (len(parents), 3)
        or radii.shape != (len(parents),)
        or not np.isfinite(positions).all()
        or not np.isfinite(radii).all()
        or np.max(abs(positions)) > 1e5
        or np.any((radii < 1e-6) | (radii > 100))
    ):
        raise ValueError(
            "bounded finite metre positions and positive node radii required"
        )
    direction = positions[1:] - positions[parents[1:]]
    length = np.linalg.norm(direction, axis=1)
    if np.any(length <= 1e-8):
        raise ValueError("noncollapsed canonical skeleton edges required")
    direction /= length[:, None]
    children = [[] for _ in parents]
    for node in range(1, len(parents)):
        children[parents[node]].append(node)
    if continuations is not None:
        continuations = np.asarray(continuations)
        if (
            continuations.shape != parents.shape
            or continuations.dtype.kind not in "iu"
            or np.any(continuations < -1)
            or np.any(continuations >= len(parents))
        ):
            raise ValueError(
                "one bounded optional source continuation per node required"
            )
        selected = np.flatnonzero(continuations >= 0)
        if np.any(parents[continuations[selected]] != selected):
            raise ValueError("source continuation must be an actual directed edge")
    dominant = np.full(len(parents), -1, np.int32)
    for node, options in enumerate(children):
        if not options:
            continue
        if continuations is not None and continuations[node] >= 0:
            dominant[node] = continuations[node]
            continue
        candidates = np.array(options)
        largest = radii[candidates].max()
        candidates = candidates[radii[candidates] >= largest * (1 - 1e-12)]
        if node:
            alignment = direction[candidates - 1] @ direction[node - 1]
            candidates = candidates[alignment >= alignment.max() - 1e-12]
        dominant[node] = candidates.min()
    starts = [
        int(dominant[0]),
        *[n for n in range(1, len(parents)) if dominant[parents[n]] != n],
    ]
    paths = []
    for start in starts:
        path = [int(parents[start]), start]
        while dominant[path[-1]] >= 0:
            path.append(int(dominant[path[-1]]))
        paths.append(tuple(path))
    return tuple(paths)
