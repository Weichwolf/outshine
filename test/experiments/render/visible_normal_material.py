"""Explicit homogeneous opaque GGX controls, not inferred native building bindings."""

import hashlib
import json
from pathlib import Path

import numpy as np
from material_filter import direct_ggx


def tables():
    path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    reference = json.loads(path.with_name("energy-tables.reference.json").read_text())
    if hashlib.sha256(path.read_bytes()).hexdigest() != reference["array_file_sha256"]:
        raise ValueError("fixed GGX reference table digest mismatch")
    with np.load(path, allow_pickle=False) as archive:
        result = {k: archive[k].astype(float) for k in archive.files}
    result["shape"] = tuple(map(int, result["shape"]))
    return result


def shade(normal, view, light, roughness, energy, *, mean_length=None):
    normal, view = np.asarray(normal, float), np.asarray(view, float)
    if np.any(np.linalg.norm(normal, axis=1) == 0) or np.any(
        np.linalg.norm(view, axis=1) == 0
    ):
        raise ValueError("nonzero material normal/view required")
    light = np.asarray(light, float)
    if (
        light.shape != (3,)
        or not np.isfinite(light).all()
        or np.linalg.norm(light) == 0
    ):
        raise ValueError("finite nonzero light direction required")
    light = light / np.linalg.norm(light)
    if np.any(
        np.linalg.norm(
            view / np.linalg.norm(view, axis=1, keepdims=True) + light, axis=1
        )
        < 1e-12
    ):
        raise ValueError("undefined opposite light/view control")
    roughness = float(np.float32(roughness))
    rows = np.zeros((len(normal), 20))
    rows[:, :3], rows[:, 3], rows[:, 4:7], rows[:, 7] = (
        float(np.float32(0.4)),
        roughness,
        float(np.float32(0.04)),
        1,
    )
    if mean_length is not None:
        length = np.minimum(mean_length, 1)
        a2 = roughness**4
        d = length * a2 + 2 * (1 - a2) * (1 - length)
        n = 2 * length * (1 - a2)
        rows[:, 3] = (2 * d / (n + 2 * d)) ** 0.25
    rows[:, 8:11], rows[:, 12:15], rows[:, 16:19] = normal, light, view
    result = direct_ggx(rows, energy)
    return result[:, :3] + result[:, 4:7]
