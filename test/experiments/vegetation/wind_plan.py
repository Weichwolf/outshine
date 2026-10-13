"""Camera- and mesh-independent wind modes from stable logical axis poses."""

import hashlib
import json

import numpy as np
from wind_field import WindField


def wind_plan(graph):
    keys = [key for key in graph.order if graph.by_id[key].kind == "axis"]
    if sum(graph.capacities[key] for key in keys) > 32:
        raise ValueError("wind experiment supports at most 32 complete axis modes")
    poses = [p for key in keys for p in graph.instances(key, 100_000)]
    if not poses or any(p.path[: len(poses[0].path)] != poses[0].path for p in poses):
        raise ValueError("one rooted parent-first axis hierarchy required")
    lengths = np.array([p.length for p in poses])
    phases = []
    for pose in poses:
        digest = hashlib.sha256(json.dumps([graph.seed, pose.path]).encode()).digest()
        phases.append(int.from_bytes(digest[:8], "little") / 2**64 * 2 * np.pi)
    direction = np.array([1.0, 0.3, 0.0])
    direction /= np.linalg.norm(direction)
    return WindField(
        np.array([p.origin for p in poses]),
        np.array([p.frame[:, 1] for p in poses]),
        lengths,
        np.maximum(lengths * 0.5, 0.05),
        np.array([0.12] + [0.012] * (len(poses) - 1)),
        np.array([0.35] + [0.8 + i * 0.13 for i in range(len(poses) - 1)]),
        np.array(phases),
        direction,
        float(lengths[0]),
    )
