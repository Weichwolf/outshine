"""Compile the existing TwigClusters hierarchy without complete crown leaf poses."""

import numpy as np
from outshine_ref.vegetation.curve_frame import arc_frame
from twig_clusters import TwigClusters


def cluster_recipe(source, base, leaf_limit):
    crown = source.crown
    return TwigClusters(
        crown.hosts,
        crown.counts,
        crown.variants,
        base.data["origins"],
        base.data["frames"],
        source.identity,
        crown.seed,
        leaf_limit=leaf_limit,
        carrier_length=crown.length_m,
        carrier_bend=crown.bend_rad,
        carrier_radii=(0.0005, 0.00005),
        petiole=crown.petiole_m,
    )


def compile_twigs(source, base, leaf_limit):
    plan = cluster_recipe(source, base, leaf_limit)
    groups = [plan.twigs(int(host), 32) for host in plan.host_ids]
    data = {k: np.concatenate([g[k] for g in groups]) for k in groups[0]}
    count = (plan.host_counts + leaf_limit - 1) // leaf_limit
    data["host_offsets"] = np.r_[np.uint64(0), np.cumsum(count, dtype=np.uint64)]
    data["leaf_offsets"] = np.r_[
        np.uint64(0), np.cumsum(data["counts"], dtype=np.uint64)
    ]
    data["keys"] = data["counts"].astype(np.uint16) * 16 + data["variants"]
    for key in np.unique(data["keys"]):
        n, variation = divmod(int(key), 16)
        local = plan.prototype(n, variation)
        shoot = plan.shoot(n, variation)
        for field in ("origins", "frames", "nodes"):
            data[f"p{key}_{field}"] = local[field]
        data[f"p{key}_length"] = np.asarray(shoot.length)
        end = arc_frame(shoot.length, shoot.bend, 0, np.array([0, 1]))[0]
        # Fixed .35rad bends are monotone on each local coordinate. Include wood ends.
        points = np.vstack((local["nodes"], end))
        data[f"p{key}_bounds"] = np.stack((points.min(0), points.max(0)))
    reach = plan.petiole + max(
        b.length + b.petiole + b.width / 2 * (4 / 3) * np.hypot(1, b.fold)
        for b in source.blades
    )
    data["lower"], data["upper"] = compiled_bounds(base, data, reach)
    return data, plan.recipe()


def compiled_bounds(base, data, reach):
    # The old bound also contains the retained parent carriers, before child work.
    low, high = base.data["lower"].copy(), base.data["upper"].copy()
    owner = np.repeat(np.arange(len(low)), np.diff(data["host_offsets"]).astype(int))
    for key in np.unique(data["keys"]):
        rows = np.flatnonzero(data["keys"] == key)
        lower, upper = data[f"p{key}_bounds"]
        centre = data["frames"][rows] @ ((lower + upper) / 2) + data["origins"][rows]
        extent = abs(data["frames"][rows]) @ ((upper - lower) / 2) + reach
        np.minimum.at(low, owner[rows], centre - extent)
        np.maximum.at(high, owner[rows], centre + extent)
    guard = 256 * np.finfo(float).eps * (np.maximum(abs(low), abs(high)) + 1)
    return np.nextafter(low - guard, -np.inf), np.nextafter(high + guard, np.inf)
