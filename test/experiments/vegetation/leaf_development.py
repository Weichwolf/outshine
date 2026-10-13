"""Open venation coupled to blade expansion and distance-controlled source birth."""

import numpy as np
from leaf_domain import contains_segment
from leaf_growth_map import LeafGrowthMap
from leaf_venation import VeinPlan, nearest


def insert_sources(nodes, sources, candidates, distance, method):
    _, vein_distance = nearest(nodes, candidates, method)
    source_distance = np.full(len(candidates), np.inf)
    if len(sources):
        _, source_distance = nearest(sources, candidates, method)
    accepted = []
    for index, (point, dv, ds) in enumerate(
        zip(candidates, vein_distance, source_distance)
    ):
        if min(dv, ds) <= distance:
            continue
        if accepted and np.any(
            np.linalg.norm(candidates[accepted] - point, axis=1) <= distance
        ):
            continue
        accepted.append(index)
    return np.array(accepted, dtype=np.int32)


def extend(blade, nodes, parents, active, step, method, persistence, growth):
    owners, distances = nearest(nodes, active, method)
    source_keep = distances > 2 * step
    active, owners, distances = (
        active[source_keep],
        owners[source_keep],
        distances[source_keep],
    )
    sums = np.zeros_like(nodes)
    np.add.at(sums, owners, (active - nodes[owners]) / distances[:, None])
    magnitude = np.linalg.norm(sums, axis=1)
    spawning = np.flatnonzero(magnitude > 1e-12)
    direction = sums[spawning] / magnitude[spawning, None]
    incoming = nodes[spawning] - nodes[np.maximum(parents[spawning], 0)]
    length = np.linalg.norm(incoming, axis=1)
    np.divide(incoming, length[:, None], out=incoming, where=length[:, None] > 0)
    direction += persistence * incoming
    direction /= np.linalg.norm(direction, axis=1)[:, None]
    delta = step * direction
    tolerance = min(blade.width, blade.length) / 2000
    factor = np.minimum(
        1, np.sqrt(tolerance / np.maximum(growth.chord_error(delta), 1e-30))
    )
    delta *= factor[:, None]
    physical_new = nodes[spawning] + delta
    new = growth.inverse(physical_new)
    errors = growth.chord_error(delta)
    rejected = 0
    if len(new):
        _, distances = nearest(nodes, physical_new, method)
        keep = np.array(
            [
                separation >= np.linalg.norm(d) / 4
                and contains_segment(blade, growth.inverse(nodes[parent]), point)
                for parent, point, separation, d in zip(spawning, new, distances, delta)
            ]
        )
        rejected = int(np.count_nonzero(~keep))
        new, spawning, errors = new[keep], spawning[keep], errors[keep]
    return source_keep, new, spawning, float(errors.max(initial=0)), rejected


def develop_veins(
    blade,
    candidates,
    method="tree",
    initial_scale=0.03,
    birth_fraction=0.1,
    settle=100,
    max_nodes=2048,
    persistence=0.6,
    shear=1.2,
):
    candidates = np.asarray(candidates, float)
    if (
        candidates.ndim != 3
        or candidates.shape[2] != 2
        or not np.isfinite(candidates).all()
    ):
        raise ValueError("finite growth-step/dart/xy candidate array required")
    growth_steps, darts, _ = candidates.shape
    if (
        not 2 <= growth_steps <= 160
        or not 1 <= darts <= 64
        or growth_steps * darts > 4095
    ):
        raise ValueError("bounded 2..160 steps and at most 4095 darts required")
    if not np.isfinite(shear) or not 0 <= shear <= 2:
        raise ValueError("bounded finite transverse growth shear required")
    if not np.isfinite(persistence) or not 0 <= persistence <= 0.9:
        raise ValueError("bounded directional persistence required")
    if method not in ("dense", "tree") or not 0.01 <= initial_scale <= 1:
        raise ValueError("known neighbor method and bounded initial scale required")
    if not np.isfinite(birth_fraction) or not 0.02 <= birth_fraction <= 0.5:
        raise ValueError("bounded positive birth spacing required")
    if (
        type(settle) is not int
        or not 0 <= settle <= 160
        or type(max_nodes) is not int
        or not 2 <= max_nodes <= 2048
    ):
        raise ValueError("bounded integer settling and node budgets required")
    t = candidates[..., 1] / blade.length
    width, _ = blade.width_profile(np.clip(t, 0, 1))
    if np.any((t < 0) | (t > 1) | (np.abs(candidates[..., 0]) > width)):
        raise ValueError("candidate sources must lie inside the mature blade")
    nodes, parents = np.zeros((1, 2)), np.array([-1], dtype=np.int32)
    active = np.array([[0, 0.9 * blade.length]])
    born, removed, rejected, max_error = 1, 0, 0, 0.0
    history, birth_steps = [], [0]
    reason = "step_budget"
    for iteration in range(growth_steps + settle):
        scale = initial_scale + (1 - initial_scale) * min(
            iteration / max(growth_steps - 1, 1), 1
        )
        growth = LeafGrowthMap(scale, shear * (1 - scale), blade.length, blade.width)
        physical_nodes, physical_active = growth.forward(nodes), growth.forward(active)
        step = min(blade.width, blade.length) / 200
        if iteration < growth_steps:
            ids = insert_sources(
                physical_nodes,
                physical_active,
                growth.forward(candidates[iteration]),
                birth_fraction * min(blade.width, blade.length),
                method,
            )
            active = np.vstack((active, candidates[iteration][ids]))
            physical_active = growth.forward(active)
            born += len(ids)
        keep, new, spawning, error, skipped = extend(
            blade,
            physical_nodes,
            parents,
            physical_active,
            step,
            method,
            persistence,
            growth,
        )
        removed += int(np.count_nonzero(~keep))
        active = active[keep]
        max_error, rejected = max(max_error, error), rejected + skipped
        if len(nodes) + len(new) > max_nodes:
            reason = "node_budget"
            break
        nodes = np.vstack((nodes, new))
        parents = np.concatenate((parents, spawning.astype(np.int32)))
        birth_steps.extend([iteration] * len(new))
        history.append((scale, len(nodes), len(active), born, removed))
        if iteration >= growth_steps - 1 and not len(active):
            reason = "complete"
            break
        if iteration >= growth_steps and not len(new):
            reason = "stationary"
            break
    load = (np.bincount(parents[1:], minlength=len(nodes)) == 0).astype(float)
    for i in range(len(nodes) - 1, 0, -1):
        load[parents[i]] += load[i]
    radii = min(blade.width, blade.length) * 0.0124 * (load / load.max()) ** (1 / 3)
    plan = VeinPlan(nodes, parents, radii, len(active), reason)
    assert born == removed + len(active)
    return plan, {
        "born_sources": born,
        "rejected_extensions": rejected,
        "max_growth_chord_error_m": max_error,
        "removed_sources": removed,
        "history": history,
        "node_birth_steps": birth_steps,
    }
