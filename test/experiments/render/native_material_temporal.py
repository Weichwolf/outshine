"""Measure material sampling with native direct lighting and the production TAA stage."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from functools import partial
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from material_temporal_camera import sequence
from native_terrain_material_pyramid import ROOT, pack, pack_levels, run
from PIL import Image, ImageDraw
from probe_terrain_material_substrate import optical_row
from scipy.stats import qmc
from terrain_material_filter import response
from terrain_material_litter import compose
from terrain_material_moments import pyramid as moment_pyramid
from terrain_material_moments import response as moment_response
from terrain_material_moments import sample as sample_moments
from terrain_material_pyramid import bake, filtered_components


def uniforms(size, frames, method):
    rng = np.random.default_rng(1934)
    if method == "iid":
        return rng.integers(0, 2**24, (frames, size * size, 4)).astype(
            np.float32
        ) / np.float32(2**24)
    temporal = qmc.Sobol(
        4, scramble=True, rng=np.random.default_rng(1934)
    ).random_base2(7)[:frames]
    offset = rng.random((size * size, 4))
    values = ((temporal[:, None, :] + offset[None, :, :]) % 1).astype(np.float32)
    return np.minimum(values, np.nextafter(np.float32(1), np.float32(0)))


def inputs(palette, pyramid, case, method, size, frames, moments=None):
    queries, motion, poses = sequence(size, frames, case == "motion")
    probes = np.zeros(
        (frames, size * size),
        dtype=[
            ("uniform", "<f4", 4),
            ("motion", "<f4", 2),
            ("which", "<i4"),
            ("mode", "<u4"),
        ],
    )
    if method != "moments":
        probes["uniform"] = uniforms(size, frames, method)
    probes["motion"] = motion
    probes["which"] = np.arange(frames * size * size).reshape(frames, -1)
    probes["mode"] = 1
    if case == "light":
        probes["mode"][frames // 2 :] = 2
    if method == "sobol4":
        probes["mode"] |= 4 << 8
    if method == "moments":
        probes["mode"] += 2
        sample_moments(moments, queries.reshape(-1, 8))
        packed = pack_levels(palette, moments, queries.reshape(-1, 8)).view("<u4")
    else:
        packed = pack(palette, pyramid, queries.reshape(-1, 8)).view("<u4")
    packed[319] = len(packed)
    return np.concatenate((packed, probes.view("<u4").ravel())), queries, poses


def reference(pyramid, queries, case, moments=None):
    view = np.array([0, 0.6, 1])
    view /= np.linalg.norm(view)
    result, reused = [], {}
    for frame, rows in enumerate(queries):
        mode = 2 if case == "light" and frame >= len(queries) // 2 else 1
        key = (frame if case == "motion" else 0, mode)
        if key not in reused:
            light = np.array([0.5, -0.2, 0.8] if mode == 1 else [-0.4, 0.2, 0.8])
            light /= np.linalg.norm(light)
            pixels = []
            if moments is not None:
                pixels = [
                    moment_response(fields, light[None], view)[0]
                    for fields in sample_moments(moments, rows)
                ]
            else:
                for row in rows:
                    fields, weights, _ = filtered_components(
                        pyramid,
                        np.array(pyramid.origin_m) + row[:2] * pyramid.extent_m,
                        row[2:4] * pyramid.extent_m,
                        row[4:6] * pyramid.extent_m,
                        float(row[6]),
                    )
                    pixels.append(response(fields, weights, light[None], view)[0])
            reused[key] = np.asarray(pixels)
        result.append(reused[key])
    return np.asarray(result)


def appearance(rgb, size):
    linear = np.clip(rgb.reshape(size, size, 3) * 4, 0, 1)
    srgb = np.where(
        linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
    )
    return Image.fromarray(np.uint8(srgb * 255)).resize(
        (256, 256), Image.Resampling.NEAREST
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size", type=int, default=32)
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument(
        "--methods",
        nargs="+",
        choices=("iid", "sobol", "sobol4", "moments"),
        default=("iid", "sobol", "sobol4", "moments"),
    )
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private temporal output belongs in system temp")
    sequence(args.size, args.frames, False)
    sequence(args.size, args.frames, True)
    output.mkdir(parents=True, exist_ok=False)
    commands = [
        run(
            [
                "nice",
                "-n",
                "10",
                "cmake",
                "--build",
                str(ROOT / "build"),
                "--target",
                "outshine_material_temporal_probe",
                "outshine_shaders",
                "--parallel",
                str(os.cpu_count() or 1),
            ],
            output,
            "build",
        )
    ]
    compiler = next(
        line.split("=", 1)[1]
        for line in (ROOT / "build/CMakeCache.txt").read_text().splitlines()
        if line.startswith("OUTSHINE_GLSLANG:FILEPATH=")
    )
    sources = [
        ROOT / "src/render/shaders/fullscreen.vert",
        Path(__file__).with_name("terrain_material_pyramid_probe.frag"),
    ]
    for name, source in zip(("vertex", "fragment"), sources, strict=True):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    "-I" + str(ROOT / "src/render/shaders"),
                    str(source),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    palette, layout, receipt = load_capture(args.capture)
    origin = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    pyramid = bake(
        partial(compose, **optical_row(palette, layout, 12)), origin, 0.0064, 64
    )
    moments = moment_pyramid(pyramid) if "moments" in args.methods else None
    if moments is not None:
        np.savez(output / "moments.npz", *moments)
    records = []
    canvas = Image.new("RGB", (3 * 256, 3 * len(args.methods) * 280), "white")
    draw = ImageDraw.Draw(canvas)
    for case in ("still", "motion", "light"):
        expected = None
        for method in args.methods:
            print(f"case={case} sampling={method}", flush=True)
            packed, queries, _ = inputs(
                palette, pyramid, case, method, args.size, args.frames, moments
            )
            source = output / f"{case}-{method}.u32"
            source.write_bytes(packed.tobytes())
            target = output / f"{case}-{method}.f32"
            commands.append(
                run(
                    [
                        "env",
                        "MTL_DEBUG_LAYER=1",
                        str(ROOT / "build/outshine_material_temporal_probe"),
                        str(output / "vertex.spv"),
                        str(output / "fragment.spv"),
                        str(source),
                        str(target),
                        str(args.size),
                        str(args.size),
                        str(args.frames),
                    ],
                    output,
                    f"{case}-{method}",
                )
            )
            native = np.fromfile(target, dtype="<f4").reshape(
                args.frames, args.size * args.size, 8
            )
            if not np.isfinite(native).all() or np.any(native[:, :, :3] < 0):
                raise ValueError("finite nonnegative native scene/resolve required")
            np.testing.assert_array_equal(native[0, :, :4], native[0, :, 4:])
            if method == "moments" and case != "motion":
                phases = (
                    (native[:, :, :4],)
                    if case == "still"
                    else np.array_split(native[:, :, :4], 2)
                )
                for phase in phases:
                    np.testing.assert_array_equal(
                        phase, np.broadcast_to(phase[0], phase.shape)
                    )
            if expected is None:
                expected = reference(pyramid, queries, case)
                expected.astype("<f8").tofile(output / f"{case}-reference.f64")
            raw, taa = native[:, :, :3].astype(float), native[:, :, 4:7].astype(float)
            errors = taa - expected
            formula_error = None
            if method == "moments":
                defined = reference(pyramid, queries, case, moments)
                formula_error = float(np.max(np.abs(raw - defined)))
                defined.astype("<f8").tofile(output / f"{case}-moments-defined.f64")
            records.append(
                {
                    "case": case,
                    "sampling": method,
                    "native_formula_max_error": formula_error,
                    "raw_rmse": np.sqrt(
                        np.mean((raw - expected) ** 2, axis=(1, 2))
                    ).tolist(),
                    "taa_rmse": np.sqrt(np.mean(errors**2, axis=(1, 2))).tolist(),
                    "last20_bias_rms": float(
                        np.sqrt(np.mean(errors[-20:].mean(axis=0) ** 2))
                    ),
                    "last20_deviation_rms": float(
                        np.sqrt(np.mean(np.var(errors[-20:], axis=0)))
                    ),
                    "input_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                    "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                }
            )
            row = len(records) - 1
            draw.text(
                (4, row * 280 + 4),
                f"{case} / {method} | stored expectation / native scene / native TAA",
                fill="black",
            )
            for col, image in enumerate((expected[-1], raw[-1], taa[-1])):
                canvas.paste(appearance(image, args.size), (col * 256, row * 280 + 24))
    canvas.save(output / "last-frames.png")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "palette": receipt["palette"],
        "size": args.size,
        "frames": args.frames,
        "material_bytes_float32": (
            sum(level.nbytes for level in moments)
            if moments is not None
            else pyramid.retained_bytes
        ),
        "python": sys.version,
        "numpy": np.__version__,
        "records": records,
        "source_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__),
                Path(__file__).with_name("material_temporal_camera.py"),
                Path(__file__).with_name("terrain_material_moments.py"),
                Path(__file__).with_name("terrain_material_moments.glsl"),
                sources[1],
                sources[1].with_name("terrain_material_sampling.glsl"),
                ROOT / "src/render/shaders/temporalResolve.frag",
                ROOT / "src/render/shaders/temporalHistory.glsl",
                ROOT / "src/render/shaders/temporalReprojection.glsl",
                ROOT / "src/render/shaders/metalRoughBrdf.glsl",
                ROOT / "build/shaders/temporalResolve.frag.spv",
                ROOT / "src/render/stages/TonemapStage.cpp",
                ROOT / "test/harness/render/NativeMaterialTemporalProbe.cpp",
                ROOT / "test/harness/render/NativeMaterialCapture.h",
                ROOT / "build/outshine_material_temporal_probe",
            )
        },
        "limitations": [
            "Controlled orthographic material plane, direct GGX only; not a Place or world integration.",
            "Reference is the stored approximate material distribution, not unfiltered source ground truth.",
            "Float32 diagnostic buffers, no MSAA, no production frame-time/UMA/Iris acceptance.",
            "Motion reprojection and light changes tested; no depth disocclusion, sky or geometry replacement.",
            "Moment candidate uses dense Float32 storage and centre-trilinear filtering; no anisotropic reconstruction or production format acceptance.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
