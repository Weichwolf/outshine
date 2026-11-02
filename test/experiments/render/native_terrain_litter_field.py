"""Qualify prepared R8 mip chains through native fragment texture sampling."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from native_terrain_material_pyramid import ROOT, run
from probe_terrain_material_substrate import optical_row
from terrain_litter_field import LitterField, bake, sample
from terrain_material_litter import footprint_field


def queries(field, anchor):
    rng = np.random.default_rng(1934)
    at = anchor + rng.uniform(-4, 4, (32, 2)) * field.scale_m
    blocks = []
    for width in (0, 0.005, 0.05, 0.25, 0.75, 1.5, 1.99, 2, 4):
        for angle, ratio, shear in (
            (0, 1, 0),
            (0.731, 1, 0),
            (0.731, 16, 0),
            (0.731, 4, 0.7),
        ):
            u = np.array([np.cos(angle), np.sin(angle)])
            v = np.array([-np.sin(angle), np.cos(angle)])
            for bias in (0, -0.5):
                q = np.zeros((len(at), 12), dtype="<f4")
                q[:, :2] = (at - field.origin_m) / field.extent_m
                q[:, 2:4] = u * width * field.scale_m / field.extent_m
                q[:, 4:6] = (
                    (v / ratio + shear * u) * width * field.scale_m / field.extent_m
                )
                q[:, 6:8], q[:, 9:11] = [field.scale_m / field.extent_m, bias], [1, 8]
                blocks.append(q)
    far = np.zeros((1, 12), dtype="<f4")
    far[0, :2], far[0, 2], far[0, 6], far[0, 9:11] = [1e6, 1e6], 2 / 32, 1 / 32, [1, 8]
    blocks.append(far)
    expected_centres = []
    for mip, values in enumerate(field.levels):
        xy = rng.integers(0, len(values), (8, 2))
        q = np.zeros((len(xy), 12), dtype="<f4")
        q[:, :2], q[:, 8] = (xy + 0.5) / len(values), mip
        blocks.append(q)
        expected_centres.extend(values[xy[:, 1], xy[:, 0]].astype(float) / 255)
    return np.concatenate(blocks), np.array(expected_centres)


def capture(output, name, field, q, commands):
    source, texture, target = [
        output / (name + suffix) for suffix in (".f32", ".r8", ".native.f32")
    ]
    q.tofile(source)
    texture.write_bytes(b"".join(level.tobytes() for level in field.levels))
    call = [
        "env",
        "MTL_DEBUG_LAYER=1",
        str(ROOT / "build/outshine_material_fragment_probe"),
        str(output / "vertex.spv"),
        str(output / "fragment.spv"),
        str(source),
        str(target),
        str(len(q)),
        str(texture),
        str(len(field.levels[0])),
    ]
    commands.append(run(call, output, name))
    native = np.fromfile(target, dtype="<f4").reshape(len(q), 8)
    if not np.isfinite(native).all():
        raise ValueError("native R8 sampling returned nonfinite data")
    commands.append(run(call, output, name + "-replay"))
    np.testing.assert_array_equal(
        native.view("<u4"), np.fromfile(target, dtype="<u4").reshape(len(q), 8)
    )
    return native


def compare(output, capture_path, commands):
    palette, layout, receipt = load_capture(capture_path)
    anchor = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    scales = sorted(
        {
            float(optical_row(palette, layout, row)["recipe"][2])
            for row in range(layout["rows"])
            if 0 < optical_row(palette, layout, row)["coverage"] < 1
        }
    )
    records = []
    for index, scale in enumerate(scales):
        origin = np.floor(anchor / (16 * scale)) * (16 * scale) - 8 * scale
        field = bake(origin, scale, dtype="uint8")
        q, expected_centres = queries(field, anchor)
        native = capture(output, f"field-{index}-hardware", field, q, commands)
        manual_queries = q.copy()
        manual_queries[manual_queries[:, 9] == 1, 9] = 3
        manual = capture(
            output, f"field-{index}-explicit", field, manual_queries, commands
        )
        direct = q[:, 9] == 0
        centre_error = float(np.max(np.abs(native[direct, 0] - expected_centres)))
        if centre_error > 2 * np.finfo(np.float32).eps:
            raise ValueError(
                "R8 conversion at exact texel centres exceeds two Float32 eps"
            )
        expected = np.zeros(np.count_nonzero(~direct))
        original = np.zeros_like(expected)
        rows = q[~direct]
        for bias in np.unique(rows[:, 7]):
            selected = rows[:, 7] == bias
            r = rows[selected].astype(float)
            at = np.array(field.origin_m) + r[:, :2] * field.extent_m
            dx, dy = r[:, 2:4] * field.extent_m, r[:, 4:6] * field.extent_m
            expected[selected] = sample(field, at, dx, dy, lod_bias=float(bias))
            original[selected] = footprint_field(at, dx, dy, field.scale_m)
        expected.tofile(output / f"field-{index}.expected.f64")
        original.tofile(output / f"field-{index}.original.f64")
        mask = native[~direct]
        if (
            np.any(np.abs(mask[:, 0]) > 0.500001)
            or np.any(mask[:, 2] < 0)
            or np.any(mask[:, 2] > 8)
        ):
            raise ValueError("native mask range or bounded tap count violated")
        np.testing.assert_array_equal(mask[-1], np.zeros(8, dtype=np.float32))
        np.testing.assert_array_equal(
            manual[~direct][-1], np.zeros(8, dtype=np.float32)
        )
        records.append(
            {
                "scale_m": scale,
                "origin_m": list(field.origin_m),
                "queries": len(q),
                "mask_queries": len(mask),
                "centre_queries": int(direct.sum()),
                "texel_centre_max_error": centre_error,
                "native_vs_cpu_max": float(np.max(np.abs(mask[:, 0] - expected))),
                "native_vs_original_max": float(np.max(np.abs(mask[:, 0] - original))),
                "explicit_vs_cpu_max": float(
                    np.max(np.abs(manual[~direct, 0] - expected))
                ),
                "explicit_vs_original_max": float(
                    np.max(np.abs(manual[~direct, 0] - original))
                ),
                "retained_texel_bytes": field.retained_bytes,
                "upload_bytes_with_256_byte_rows": sum(
                    ((len(v) + 255) // 256) * 256 * len(v) for v in field.levels
                ),
            }
        )
    return receipt, records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private native outputs belong in system temp")
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
                "outshine_material_fragment_probe",
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
        Path(__file__).with_name("terrain_litter_texture_probe.frag"),
    ]
    for name, path in zip(("vertex", "fragment"), sources, strict=True):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    str(path),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    control = LitterField(
        (0, 0),
        32,
        1,
        tuple(
            np.full((512 >> mip,) * 2, 17 + 23 * mip, dtype=np.uint8)
            for mip in range(10)
        ),
    )
    q = np.zeros((296, 12), dtype="<f4")
    q[:, :2] = np.random.default_rng(0).uniform(0.1, 0.9, (len(q), 2))
    q[:148, 8] = np.repeat(np.arange(37) / 4, 4)
    q[148:, 8] = np.random.default_rng(1).uniform(0, 9, 148)
    result = capture(output, "mip-control", control, q, commands)
    expected = (17 + 23 * q[:, 8].astype(float)) / 255
    control_error = float(np.max(np.abs(result[:148, 0] - expected[:148])))
    if control_error > 8 * np.finfo(np.float32).eps:
        raise ValueError(
            "constant-mip interpolation exceeds eight Float32 rounding eps"
        )
    q[:, 9] = 2
    explicit = capture(output, "mip-control-explicit", control, q, commands)
    explicit_error = float(np.max(np.abs(explicit[:, 0] - expected)))
    if explicit_error > 8 * np.finfo(np.float32).eps:
        raise ValueError(
            "explicit constant-mip interpolation exceeds eight Float32 rounding eps"
        )
    receipt, records = compare(output, args.capture, commands)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "palette": receipt["palette"],
        "mip_control_max_error": control_error,
        "arbitrary_lod_hardware_error": float(np.max(np.abs(result[:, 0] - expected))),
        "arbitrary_lod_explicit_error": explicit_error,
        "records": records,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__),
                sources[1],
                Path(__file__).with_name("terrain_litter_sampling.glsl"),
                Path(__file__).with_name("terrain_litter_field.py"),
                ROOT / "test/harness/render/NativeMaterialFragmentProbe.cpp",
                ROOT / "test/harness/render/NativeMaterialCaptureTexture.h",
                ROOT / "build/outshine_material_fragment_probe",
            ]
        },
        "limitations": [
            "Native immutable R8 texture/mip/filter qualification only; no AssetDB or world integration.",
            "Hardware interpolation precision is measured separately from scalar-field approximation.",
            "No material/sky/TAA/world frame-time or Iris acceptance.",
        ],
    }
    report["artifact_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in output.iterdir()
        if p.suffix in (".f32", ".f64", ".r8", ".spv")
    }
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
