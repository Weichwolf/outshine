"""Bounded open vein graphs in physical leaf coordinates, with equivalent neighbor searches."""

from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree


def nearest(nodes, sources, method):
    if method == "dense":
        if len(nodes) * len(sources) > 8_388_608:
            raise ValueError("dense vein search exceeds its declared pair budget")
        distance2 = np.sum((sources[:, None] - nodes[None]) ** 2, axis=2)
        owners = np.argmin(distance2, axis=1)
        return owners, np.sqrt(distance2[np.arange(len(sources)), owners])
    if method != "tree":
        raise ValueError("dense or tree vein search required")
    tree = cKDTree(nodes)
    distance, owners = tree.query(sources, k=2 if len(nodes) > 1 else 1)
    if len(nodes) == 1:
        return owners, distance
    tied = np.flatnonzero(np.abs(distance[:, 0] - distance[:, 1]) <= 1e-14)
    selected = owners[:, 0].copy()
    for index in tied:
        candidates = tree.query_ball_point(sources[index], distance[index, 0] + 1e-14)
        squared = np.sum((nodes[candidates] - sources[index]) ** 2, axis=1)
        order = np.lexsort((np.array(candidates), squared))
        selected[index] = candidates[order[0]]
    exact = np.linalg.norm(sources - nodes[selected], axis=1)
    return selected, exact


def leaf_sources(blade, seed, count):
    if type(count) is not int or not 1 <= count <= 4096:
        raise ValueError("1..4096 leaf attraction sources required")
    rng = np.random.Generator(np.random.PCG64(seed))
    candidates = rng.uniform(
        (-0.5 * blade.width, 0), (0.5 * blade.width, blade.length), (count * 8, 2)
    )
    width, _ = blade.width_profile(candidates[:, 1] / blade.length)
    points = candidates[np.abs(candidates[:, 0]) <= width]
    if len(points) < count:
        raise ValueError("bounded source sampling did not fill the requested leaf")
    return points[:count]


@dataclass(frozen=True)
class VeinPlan:
    positions: np.ndarray
    parents: np.ndarray
    radii: np.ndarray
    remaining_sources: int
    stop_reason: str


def grow_veins(blade, sources, method="tree", max_nodes=2048, steps=160):
    sources = np.asarray(sources, dtype=float)
    if sources.ndim != 2 or sources.shape[1] != 2 or not np.isfinite(sources).all():
        raise ValueError("finite Nx2 leaf sources required")
    if len(sources) > 4096 or type(max_nodes) is not int or not 32 <= max_nodes <= 2048:
        raise ValueError("bounded complete source/node budgets required")
    if type(steps) is not int or not 0 <= steps <= 160:
        raise ValueError("bounded integer growth step count required")
    if method not in ("dense", "tree"):
        raise ValueError("dense or tree vein search required")
    source_u = sources[:, 1] / blade.length
    source_width, _ = blade.width_profile(np.clip(source_u, 0, 1))
    if np.any((source_u < 0) | (source_u > 1) | (np.abs(sources[:, 0]) > source_width)):
        raise ValueError("vein attraction sources must lie inside the leaf")
    nodes = np.zeros((1, 2))
    parents = np.array([-1], dtype=np.int32)
    step = min(blade.length, blade.width) / 40
    kill = step * 1.6
    active = sources.copy()
    reason = "step_budget"
    for _ in range(steps):
        if not len(active):
            reason = "complete"
            break
        owners, distance = nearest(nodes, active, method)
        keep = distance > kill
        active, owners, distance = active[keep], owners[keep], distance[keep]
        if not len(active):
            reason = "complete"
            break
        sums = np.zeros_like(nodes)
        np.add.at(sums, owners, (active - nodes[owners]) / distance[:, None])
        magnitude = np.linalg.norm(sums, axis=1)
        spawning = np.flatnonzero(magnitude > 1e-12)
        new = nodes[spawning] + step * sums[spawning] / magnitude[spawning, None]
        u = new[:, 1] / blade.length
        widths, _ = blade.width_profile(np.clip(u, 0, 1))
        inside = (u >= 0) & (u <= 1) & (np.abs(new[:, 0]) <= widths)
        new, spawning = new[inside], spawning[inside]
        if len(new):
            _, separation = nearest(nodes, new, method)
            distinct = separation >= step * 0.25
            new, spawning = new[distinct], spawning[distinct]
        if not len(new):
            reason = "stationary"
            break
        if len(nodes) + len(new) > max_nodes:
            reason = "node_budget"
            break
        nodes = np.vstack((nodes, new))
        parents = np.concatenate((parents, spawning.astype(np.int32)))
    if len(active):
        _, distance = nearest(nodes, active, method)
        active = active[distance > kill]
    if not len(active):
        reason = "complete"
    load = (np.bincount(parents[1:], minlength=len(nodes)) == 0).astype(float)
    for i in range(len(nodes) - 1, 0, -1):
        load[parents[i]] += load[i]
    radii = min(blade.width, blade.length) * 0.0124 * (load / load.max()) ** (1 / 3)
    return VeinPlan(nodes, parents, radii, len(active), reason)
