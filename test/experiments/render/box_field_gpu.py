"""Bounded codec and native build/run for the box-field query comparison."""

import hashlib
import json
import math
import os
import platform
import struct
import subprocess
from pathlib import Path

import numpy as np
from native_tree_detail_world import checked, compile_unit, libraries

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MAGIC = 0x32425155
PARAMETER_BYTES = 304


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def encode(field, ray_data, *, frame=None):
    if field.palette is None:
        raise ValueError("GPU comparison requires at most 256 exact height values")
    rays = np.asarray(ray_data, dtype="<f4")
    if frame is None:
        if rays.ndim != 3 or rays.shape[1:] != (2, 4):
            raise ValueError("explicit rays require two vec4s each")
        if np.any(np.linalg.norm(rays[:, 1, :3], axis=1) == 0):
            raise ValueError("ray directions must be nonzero")
        count, view = len(rays), [0, 0, 0, 0]
    else:
        width, height = frame
        if rays.shape != (4, 4) or min(width, height) <= 0:
            raise ValueError("camera requires four vec4s and positive dimensions")
        count, view = width * height, [width, height, 1, 0]
    if not 0 < count <= 2097152 or not np.isfinite(rays).all():
        raise ValueError("bounded nonempty finite ray input required")
    storage = bytearray(
        np.pad(field.palette, (0, 256 - len(field.palette))).astype("<f4").tobytes()
    )
    levels = np.zeros((16, 4), "<u4")
    for level, mip in enumerate(field.mips):
        levels[level] = [len(storage), mip.shape[1], mip.shape[1] - 1, mip.shape[0] - 1]
        storage.extend(mip.tobytes())
    storage.extend(bytes(-len(storage) % 4))
    metric = np.array([field.pitch, field.fill, field.maximum_height, 0.1], "<f4")
    if not np.isfinite(metric).all() or metric[0] <= 0:
        raise ValueError("field metric must remain finite in Float32")
    parameters = (
        np.array([field.columns, field.rows, field.top, count], "<u4").tobytes()
        + metric.tobytes()
        + np.array(view, "<u4").tobytes()
        + levels.tobytes()
    )
    assert len(parameters) == PARAMETER_BYTES
    source = (
        struct.pack("<4I", len(storage) // 4, rays.size, MAGIC, 0)
        + parameters
        + storage
        + rays.tobytes()
    )
    if len(source) > 64 * 1024 * 1024:
        raise ValueError("query input exceeds the native byte budget")
    return source, {
        "field_bytes": len(storage),
        "ray_bytes": rays.nbytes,
        "uniform_bytes": PARAMETER_BYTES,
        "output_bytes": count * 16,
    }


def build(native, output, glslang):
    database = json.loads((native / "compile_commands.json").read_text())
    source = HERE / "urban_fields/NativeBoxFieldQuery.cpp"
    flags = compile_unit(database, native, "Engine.cpp", source, output / "query.o")
    flags[1:1] = [f"-I{native}/test/harness/{part}" for part in ("render", "shared")]
    checked(["nice", "-n", "10", *flags], native, output / "compile.log", 180)
    link = [
        flags[0],
        *[f for f in flags[1:] if f.startswith("-stdlib=")],
        str(output / "query.o"),
        str(native / "build/liboutshine.a"),
        *libraries(native),
        "-o",
        str(output / "query"),
    ]
    checked(["nice", "-n", "10", *link], native, output / "link.log", 180)
    commands = {}
    for method in ("direct", "hierarchy", "direct-work", "hierarchy-work"):
        command = [str(glslang), "-V", f"-I{HERE}/urban_fields"]
        if method.startswith("hierarchy"):
            command.append("-DBOX_QUERY_HIERARCHY=1")
        if method.endswith("-work"):
            command.append("-DBOX_QUERY_WORK=1")
        command += [
            str(source.with_name("BoxFieldQuery.comp")),
            "-o",
            str(output / f"{method}.spv"),
        ]
        checked(command, native, output / f"{method}-compile.log")
        commands[method] = command
    paths = [
        source,
        source.with_name("BoxFieldQuery.comp"),
        source.with_name("BoxFieldQuery.glsl"),
        Path(__file__),
        HERE / "bench_box_field_gpu.py",
        HERE / "box_field_query.py",
        HERE / "column_field_query.py",
        HERE / "bench_box_field.py",
        HERE / "triangle_image_reference.py",
    ]
    return {
        "compile": flags,
        "link": link,
        "shaders": commands,
        "source_sha256": {str(p.relative_to(ROOT)): sha(p) for p in paths},
        "library_sha256": sha(native / "build/liboutshine.a"),
        "binary_sha256": sha(output / "query"),
        "shader_sha256": {m: sha(output / f"{m}.spv") for m in commands},
    }


def run(native, output, source, prefix, method, *, validation=False):
    environment = dict(os.environ)
    for key in (
        "MTL_DEBUG_LAYER",
        "MTL_SHADER_VALIDATION",
        "OUTSHINE_METAL_ENCODER_PROFILE",
        "VK_INSTANCE_LAYERS",
        "VK_LOADER_LAYERS_ENABLE",
        "VK_LAYER_ENABLES",
        "VK_LAYER_VALIDATE_SYNC",
    ):
        environment.pop(key, None)
    if validation:
        if platform.system() == "Darwin":
            environment.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
        else:
            environment["VK_LAYER_VALIDATE_SYNC"] = "1"
    command = [
        str(output / "query"),
        str(output / f"{method}.spv"),
        str(source),
        str(prefix),
        "validate" if validation else "normal",
    ]
    with Path(f"{prefix}.log").open("w") as log:
        subprocess.run(
            command,
            cwd=native,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=120,
        )
    text = Path(f"{prefix}.log").read_text()
    if any(
        marker in text
        for marker in ("VUID-", "SYNC-HAZARD", "Validation Error", "failed assertion")
    ):
        raise RuntimeError(f"GPU validation diagnostic: {prefix}.log")
    device = next(
        row.removeprefix("DEVICE ")
        for row in text.splitlines()
        if row.startswith("DEVICE ")
    )
    times = np.loadtxt(f"{prefix}.times")
    if times.shape != (60,) or not np.isfinite(times).all() or np.any(times < 0):
        raise ValueError("sixty finite GPU submission times required")
    ordered = np.sort(times)
    hits = np.fromfile(f"{prefix}.hits", "<f4").reshape(-1, 4)
    words = hits.view("<u4")
    if words[:, 3].any() or np.any(words[:, 1] > 6):
        raise ValueError("invalid hit or traversal exceeded its bounded work")
    normal = np.zeros((len(hits), 3))
    keep = np.flatnonzero(words[:, 1])
    normal[keep, (words[keep, 1] - 1) // 2] = np.where(words[keep, 1] % 2, -1, 1)
    record = {
        "command": command,
        "returncode": 0,
        "device": device,
        "validation": validation,
        "gpu_submission_ms": {
            "p50": float(np.median(times)),
            "p95": float(ordered[math.ceil(0.95 * len(times)) - 1]),
            "p99": float(ordered[math.ceil(0.99 * len(times)) - 1]),
        },
        "instrumented": method.endswith("-work"),
        "node_evaluations": int(words[:, 2].sum())
        if method.endswith("-work")
        else None,
        "overflow_rays": 0,
    }
    return hits[:, 0], normal, record
