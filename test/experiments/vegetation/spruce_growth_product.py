"""Complete cached axes/windows with real graph paths and demand-driven needles."""

import hashlib
import json

import numpy as np
from conifer_windows import bind_patterns, pattern_key
from outshine_ref.vegetation import AxisState
from outshine_ref.vegetation.needle_windows import _axis_window_values


def pack_axes(poses, counts):
    lengths = np.array([len(pose.path) for pose in poses], dtype=np.uint64)
    return {
        "origins": np.array([p.origin for p in poses]),
        "rotations": np.array([p.frame for p in poses]),
        "uniform_scales": np.array([p.scale for p in poses]),
        "lengths": np.array([p.length for p in poses]),
        "bends": np.array([p.bend for p in poses]),
        "path_offsets": np.r_[np.uint64(0), np.cumsum(lengths, dtype=np.uint64)],
        "path_pairs": np.array(
            [pair for pose in poses for pair in pose.path], dtype=np.uint32
        ),
        "needle_counts": np.asarray(counts, dtype=np.uint32),
        "first_needle_ids": np.r_[
            np.uint64(0), np.cumsum(counts, dtype=np.uint64)[:-1]
        ],
    }


def support(axes, index):
    lo, hi = axes["path_offsets"][index : index + 2]
    path = tuple(int(slot) for slot in axes["path_pairs"][int(lo) + 2 : int(hi), 1])
    return AxisState(
        path,
        int(index),
        float(axes["lengths"][index]),
        float(axes["bends"][index]),
        axes["origins"][index],
        axes["rotations"][index],
        0,
        0,
        path_capacity=256,
    )


def windows(axes, patterns, catalog, seed, expected):
    needed = 0
    active = np.flatnonzero(axes["needle_counts"])
    orders = np.diff(axes["path_offsets"]).astype(np.int64) - 2
    for order, pattern in enumerate(patterns):
        if pattern is not None:
            capacity = catalog[pattern_key(pattern)]["needles"]
            count = axes["needle_counts"][orders == order].astype(np.uint64)
            needed += int(((count + capacity - 1) // capacity).sum())
    if needed != expected or not 0 < needed <= 80000:
        raise ValueError("complete window budget required before allocation")
    columns = {}
    for index in active:
        axis = support(axes, index)
        pattern = patterns[int(orders[index])]
        capacity = catalog[pattern_key(pattern)]["needles"]
        values = _axis_window_values(
            axis,
            pattern,
            int(axes["needle_counts"][index]),
            capacity,
            seed,
            int(axes["first_needle_ids"][index]),
        )
        for name, value in values.items():
            columns.setdefault(name, []).append(value)
    raw = {name: np.concatenate(rows) for name, rows in columns.items()}
    if len(raw["origins"]) != expected:
        raise ValueError("complete predicted window rows required")
    return bind_patterns(patterns, raw, catalog)


def publish(output, recipe, execution, execution_report, catalog):
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    (output / "plant-recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")
    paths = [execution / n for n in ("graph.json", "axes.npz", "execution.json")]
    paths += [
        catalog / "source.json",
        catalog / "catalog.json",
        *sorted(catalog.glob("family-*.npz")),
    ]
    manifest = {
        "version": "cached-window-plant-1",
        "plan_identity": identity,
        "recipe": "plant-recipe.json",
        "source_scope": "Complete experimental parent-profile axes and physical windows; reusable finite catalog, no whole-tree atlas or organ expansion. Unapproved botanical/material/window/LOD fidelity.",
        "axes": execution_report["axes"],
        "windows": execution_report["windows"],
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
    }
    (output / "plant-product.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest
