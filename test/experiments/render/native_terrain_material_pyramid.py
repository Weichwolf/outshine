"""Run one-sample static material filtering through the production native compute API."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from functools import partial
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from probe_terrain_material_substrate import optical_row
from terrain_material_litter import compose
from terrain_material_pyramid import bake

ROOT = Path(__file__).resolve().parents[3]


def run(command, output, name):
    with (output / (name + ".log")).open("w") as stream:
        subprocess.run(
            command,
            cwd=ROOT,
            stdout=stream,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=180,
        )
    return command


def pack(palette, pyramid, queries):
    prefix = np.zeros(320 + 4 + 4 * len(pyramid.levels), dtype="<f4")
    prefix[: len(palette)] = palette
    words = prefix.view("<u4")
    words[320:324] = [len(pyramid.levels), 324, 0, len(queries)]
    blocks, offset = [], len(prefix)
    for index, level in enumerate(pyramid.levels):
        width, _, capacity, _ = level.fields.shape
        records = np.zeros((width, width, capacity, 8), dtype="<f4")
        records[..., :6], records[..., 6] = level.fields, level.weights
        words[324 + index * 4 : 328 + index * 4] = [width, capacity, offset, 0]
        blocks.append(records.ravel())
        offset += records.size
    words[322] = offset
    return np.concatenate([prefix, *blocks, queries.ravel()])


def expected(pyramid, queries, uniforms, which):
    """Defined Float32 addressing arithmetic; stored components are copied exactly."""
    query = queries[which]
    lod = query[:, 6]
    base = np.floor(lod).astype(int)
    indices = base + (uniforms[:, 0] < lod - base.astype(np.float32))
    local = (
        query[:, :2]
        + (uniforms[:, 1:2] - np.float32(0.5)) * query[:, 2:4]
        + (uniforms[:, 2:3] - np.float32(0.5)) * query[:, 4:6]
    )
    result = np.empty((len(query), 8), dtype="<f4")
    for index in np.unique(indices):
        active = indices == index
        level = pyramid.levels[index]
        width, _, capacity, _ = level.fields.shape
        xy = np.minimum(
            np.floor(local[active] * np.float32(width)).astype(int), width - 1
        )
        weights = level.weights[xy[:, 1], xy[:, 0]].astype(np.float32)
        cumulative = np.cumsum(weights, axis=-1, dtype=np.float32)
        wanted = uniforms[active, 3:4] * cumulative[:, -1:]
        atom = np.minimum(np.sum(wanted >= cumulative, axis=-1), capacity - 1)
        result[active, :6] = level.fields[xy[:, 1], xy[:, 0], atom]
        result[active, 6], result[active, 7] = (
            index,
            (xy[:, 1] * width + xy[:, 0]) * capacity + atom,
        )
    return result


def prepare(capture, output):
    palette, layout, receipt = load_capture(capture)
    origin = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    pyramid = bake(
        partial(compose, **optical_row(palette, layout, 12)), origin, 0.0064, 64
    )
    queries = []
    for x in (0.43, 0.51, 0.57):
        for angle, anisotropy in ((0, 1), (0.731, 1), (0.731, 16)):
            dx, dy = np.array(
                [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
            ) * [[0.23], [0.23 / anisotropy]]
            for lod in (0.25, 2.5, 4.75):
                queries.append([x, 0.5, *dx, *dy, lod, 0])
    queries = np.asarray(queries, dtype="<f4")
    count = 115200
    probes = np.zeros(
        count + 2,
        dtype=[
            ("uniform", "<f4", 4),
            ("unused", "<f4", 2),
            ("which", "<i4"),
            ("mode", "<u4"),
        ],
    )
    if probes.itemsize != 32 or [
        probes.dtype.fields[key][1] for key in probes.dtype.names
    ] != [0, 16, 24, 28]:
        raise ValueError("existing native probe ABI changed")
    probes["uniform"] = np.random.default_rng(1934).integers(
        0, 2**24, (count + 2, 4)
    ).astype(np.float32) / np.float32(2**24)
    probes["which"] = np.arange(count + 2) % len(queries)
    probes["which"][-2:] = [-1, len(queries)]
    values = np.zeros((count + 2, 8), dtype="<f4")
    values[:count] = expected(
        pyramid, queries, probes["uniform"][:count], probes["which"][:count]
    )
    packed = pack(palette, pyramid, queries)
    packed.tofile(output / "parameters.f32")
    probes.tofile(output / "probes.bin")
    values.tofile(output / "expected.f32")
    return receipt, packed.nbytes, probes.nbytes, len(queries)


def build(output):
    command = [
        "nice",
        "-n",
        "10",
        "cmake",
        "--build",
        str(ROOT / "build"),
        "--target",
        "outshine_material_compute_probe",
        "--parallel",
        str(os.cpu_count() or 1),
    ]
    run(command, output, "build")
    return ROOT / "build/outshine_material_compute_probe", [command]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private native probe output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    shader_compiler = next(
        (
            line.split("=", 1)[1]
            for line in (ROOT / "build/CMakeCache.txt").read_text().splitlines()
            if line.startswith("OUTSHINE_GLSLANG:FILEPATH=")
        ),
        shutil.which("glslangValidator") or shutil.which("glslang"),
    )
    if not shader_compiler or not Path(shader_compiler).is_file():
        raise RuntimeError("configured CMake shader compiler unavailable")
    receipt, field_bytes, probe_bytes, queries = prepare(args.capture, output)
    binary, commands = build(output)
    shader = Path(__file__).with_name("terrain_material_pyramid_probe.comp")
    commands += [
        run(
            [
                shader_compiler,
                "-V",
                "--target-env",
                "vulkan1.0",
                str(shader),
                "-o",
                str(output / "probe.spv"),
            ],
            output,
            "shader",
        )
    ]
    for label, timing in (("plain", []), ("timed", ["--gpu-timing"])):
        commands.append(
            run(
                [
                    "env",
                    "MTL_DEBUG_LAYER=1",
                    str(binary),
                    str(output / "probe.spv"),
                    str(output / "parameters.f32"),
                    str(output / "probes.bin"),
                    str(output / (label + ".f32")),
                    "0",
                    *timing,
                ],
                output,
                label,
            )
        )
        actual = np.fromfile(output / (label + ".f32"), dtype="<f4")
        expected_values = np.fromfile(output / "expected.f32", dtype="<f4")
        np.testing.assert_array_equal(actual.view("<u4"), expected_values.view("<u4"))
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "palette": receipt["palette"],
        "queries": queries,
        "samples": probe_bytes // 32,
        "invalid_query_controls": 2,
        "field_and_query_bytes": field_bytes,
        "probe_bytes": probe_bytes,
        "commands": commands,
        "gpu_equals_defined_float32_reference": True,
        "timed_equals_plain": True,
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "python": sys.version,
        "numpy": np.__version__,
        "input_sha256": {
            name: hashlib.sha256((output / name).read_bytes()).hexdigest()
            for name in (
                "parameters.f32",
                "probes.bin",
                "expected.f32",
                "probe.spv",
            )
        },
        "native_log": (output / "plain.log").read_text(),
        "timing_log": (output / "timed.log").read_text(),
        "source_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__),
                shader,
                Path(__file__).with_name("terrain_material_pyramid.py"),
                Path(__file__).with_name("terrain_material_filter.py"),
                Path(__file__).with_name("terrain_material_litter.py"),
                Path(__file__).with_name("terrain_material_substrate.py"),
                ROOT / "test/harness/render/NativeMaterialComputeProbe.cpp",
                ROOT / "build/liboutshine.a",
            )
        },
        "limitations": [
            "Packed experiment inputs, not a native AssetDB format or product integration.",
            "Native sampling/compute cost only; no lighting, TAA variance, frame-time or Iris proof.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
