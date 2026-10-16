"""Check every unique tested normal/direction pair through the original GPU probe."""

import hashlib
import os

import numpy as np
from facade_coating_raster import ROOT, run


def dots(binary, output, models, lights):
    unique = np.unique(
        np.concatenate(
            [
                np.vstack([lobes, mean])
                for group in models.values()
                for lobes, _, mean in group
            ]
        ).astype("<f4"),
        axis=0,
    )
    views = np.array(
        [[0, 0, 1], [np.sin(np.deg2rad(50)), 0, np.cos(np.deg2rad(50))]], dtype="<f4"
    )
    directions = np.tile(lights.astype("<f4"), (2, 1))
    view_pairs = np.repeat(views, len(lights), axis=0)
    pairs = len(directions)
    records = np.zeros((len(unique) * pairs, 12), dtype="<f4")
    records[:, :3] = np.repeat(unique, pairs, axis=0)
    records[:, 3] = 0.5
    records[:, 4:7] = np.tile(directions, (len(unique), 1))
    records[:, 7] = 1
    records[:, 8:11] = np.tile(view_pairs, (len(unique), 1))
    original = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette = np.zeros(320 + records.size, dtype="<f4")
    palette[: len(original)] = original
    palette[320:] = records.ravel()
    palette.tofile(output / "dot-palette.f32")
    probes = np.zeros(
        len(records), dtype=[("data", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(probes))
    probes.tofile(output / "dot-probes.bin")
    run(
        [
            "nice",
            "-n",
            "10",
            os.environ.get("GLSLANG", "glslangValidator"),
            "-V",
            "--target-env",
            "vulkan1.0",
            "-Isrc/render/shaders",
            "test/experiments/render/relief_shading_probe.comp",
            "-o",
            str(output / "dots.spv"),
        ],
        output,
        "dot-compile",
    )
    command = [
        str(binary),
        str(output / "dots.spv"),
        str(output / "dot-palette.f32"),
        str(output / "dot-probes.bin"),
        str(output / "dots.f32"),
        "13",
    ]
    log = run(command, output, "dot-capture")
    if "Validation Error" in log or "VUID-" in log:
        raise AssertionError("dot validation failure")
    gpu = np.fromfile(output / "dots.f32", dtype="<f4").reshape(-1, 8)
    if len(gpu) != len(records) or not np.isfinite(gpu).all():
        raise AssertionError("dot output shape/nonfinite failure")
    raw = records.astype(float)
    half = raw[:, 4:7] + raw[:, 8:11]
    half /= np.linalg.norm(half, axis=1)[:, None]
    cpu = np.column_stack(
        [
            np.maximum(np.sum(raw[:, :3] * raw[:, 4:7], axis=1), 0),
            np.maximum(np.sum(raw[:, :3] * raw[:, 8:11], axis=1), 1e-6),
            np.maximum(np.sum(raw[:, :3] * half, axis=1), 0),
            np.maximum(np.sum(raw[:, 8:11] * half, axis=1), 0),
        ]
    )
    error = np.abs(cpu - gpu[:, 4:])
    if error.max() > 3e-7:
        raise AssertionError(f"fixed float32 dot bound exceeded: {error.max()}")
    return {
        "normals": len(unique),
        "pairs": len(records),
        "max_dot_error": float(error.max()),
        "fixed_dot_bound": 3e-7,
        "command": command,
        "exit": 0,
        "input_sha256": hashlib.sha256(records.tobytes()).hexdigest(),
    }
