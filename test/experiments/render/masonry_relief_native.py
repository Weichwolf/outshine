"""Execute private masonry relief candidates through the common native GPU probe."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT, run
from ground_litter_native import capture
from masonry_relief import MAX_NORMAL_ERROR_DEG, RMS_NORMAL_ERROR_DEG, SIDES
from masonry_relief_controls import authored_composition, periodic
from masonry_relief_filter import filtered, normal
from masonry_relief_reference import exact

NORMAL_PARITY_DEG = 0.05
GRADIENT_PARITY = 0.0002


def compile_shader(output, side, counted=True, authored=False):
    path = output / (
        f"filter-{side}.spv" if counted else f"filter-{side}-uncounted.spv"
    )
    if authored:
        path = output / f"filter-{side}-authored.spv"
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
            "-Itest/experiments/render",
            f"-DRELIEF_SIDE={side}",
            f"-DRELIEF_COUNT_WORK={int(counted)}",
            f"-DRELIEF_AUTHORED_NORMAL={int(authored)}",
            "test/experiments/render/masonry_relief_probe.comp",
            "-o",
            str(path),
        ],
        output,
        f"compile-{path.stem}",
    )
    return path


def inputs(source, output):
    with np.load(source / "inputs.npz") as data:
        records = np.column_stack(
            [
                data["at"],
                data["dx"],
                data["dy"],
                data["fractions"],
                data["per_m"],
                np.full(len(data["at"]), 0.002),
            ]
        ).astype("<f4")
    originals = len(records)
    zero = records[:8].copy()
    zero[:, 15] = 0
    records = np.concatenate([records, zero])
    palette = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    expanded = np.zeros(320 + records.size, dtype="<f4")
    expanded[: len(palette)] = palette
    expanded[320:] = records.ravel()
    expanded.tofile(output / "native.palette")
    probes = np.zeros(
        len(records), dtype=[("values", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(records))
    probes.tofile(output / "probes.bin")
    return records.astype(float), originals, probes


def angle(a, b):
    return np.rad2deg(2 * np.arcsin(np.minimum(np.linalg.norm(a - b, axis=-1) / 2, 1)))


def profile(binary, output, actual, probes):
    destination = output / "cost"
    destination.mkdir(exist_ok=True)
    indices = np.arange(115200) % len(probes)
    probes[indices].tofile(destination / "probes.bin")
    rows = []
    uncounted = compile_shader(output, 4, False)
    methods = (
        (1, output / "filter-1.spv"),
        (4, output / "filter-4.spv"),
        (4, uncounted),
    )
    for order, sequence in enumerate((methods, methods[::-1])):
        for side, shader in sequence:
            name = f"{order}-{shader.stem}"
            target = destination / (name + ".f32")
            cmd = [
                str(binary),
                str(shader),
                str(output / "native.palette"),
                str(destination / "probes.bin"),
                str(target),
                "13",
                "--gpu-timing",
            ]
            text = run(cmd, output, name)
            values = np.fromfile(target, dtype="<f4").reshape(-1, 8)
            np.testing.assert_array_equal(values[:, :7], actual[side][indices, :7])
            if shader == uncounted:
                np.testing.assert_array_equal(values[:, 7], 0)
            else:
                np.testing.assert_array_equal(values, actual[side][indices])
            elapsed = re.search(
                r"profile_available=1 profile_dispatches=(\d+) profile_sum_ms=([\d.e+-]+)",
                text,
            )
            assert elapsed and int(elapsed[1]) == 32
            rows.append(
                {
                    "side": side,
                    "counted": shader != uncounted,
                    "command": cmd,
                    "exit": 0,
                    "dispatches": 32,
                    "gpu_dispatch_sum_ms": float(elapsed[2]),
                    "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                }
            )
            target.unlink()
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    records, original_count, probes = inputs(args.input.resolve(), output)
    at, dx, dy = records[:, :4], records[:, 4:8], records[:, 8:12]
    fractions, per_m = records[:, 12:14], records[:, 14]
    reference = np.array(
        [
            exact(p, u, v, f, s, depth=d)[0]
            for p, u, v, f, s, d in zip(
                at, dx, dy, fractions, per_m, records[:, 15], strict=True
            )
        ]
    )
    reference_normal = normal(reference, dx[:, 2:], dy[:, 2:])
    reference.tofile(output / "reference-quantized.f64")
    report, values = [], {}
    for side in SIDES:
        compile_shader(output, side)
        values[side] = capture(
            args.binary.resolve(), output, side, "native", len(records), 13
        )
        actual = values[side]
        predicted = filtered(at, dx, dy, fractions, per_m, side)
        predicted[original_count:] = 0
        cpu_normal = normal(predicted, dx[:, 2:], dy[:, 2:])
        parity = angle(actual[:, 4:7], cpu_normal)
        gradient_parity = float(np.abs(actual[:, :2] - predicted).max())
        assert (
            gradient_parity <= GRADIENT_PARITY and parity.max() <= NORMAL_PARITY_DEG
        ), (side, gradient_parity, float(parity.max()))
        assert np.abs(actual[:, :2]).max() <= 1.00002
        np.testing.assert_array_equal(actual[original_count:, :4], 0)
        np.testing.assert_array_equal(
            actual[original_count:, 4:7], np.tile([0, 0, 1], (8, 1))
        )
        assert actual[:, 7].min() >= 0 and actual[:, 7].max() <= max(side, 1) ** 2
        errors = angle(actual[:original_count, 4:7], reference_normal[:original_count])
        maximum, rms = float(errors.max()), float(np.sqrt(np.mean(errors * errors)))
        report.append(
            {
                "side": side,
                "gradient_parity": gradient_parity,
                "normal_parity_max_deg": float(parity.max()),
                "reference_max_deg": maximum,
                "reference_rms_deg": rms,
                "mask_evaluations": int(actual[:, 7].sum()),
                "accepted": maximum <= MAX_NORMAL_ERROR_DEG
                and rms <= RMS_NORMAL_ERROR_DEG,
            }
        )
        print(report[-1], flush=True)
    costs = (
        profile(args.binary.resolve(), output, values, probes) if args.profile else []
    )
    controls = {
        "periodic": periodic(args.binary.resolve(), output, compile_shader),
        "authored": authored_composition(
            args.binary.resolve(), output, records, values[4], compile_shader
        ),
    }
    document = {
        "cases": len(records),
        "methods": report,
        "cost": costs,
        "controls": controls,
        "source_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path("test/experiments/render").glob("masonry_relief*")
            if p.suffix in (".py", ".glsl", ".comp")
        },
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "Private native compute candidate; no production or full-frame claim",
    }
    (output / "comparison.json").write_text(json.dumps(document, indent=2) + "\n")


if __name__ == "__main__":
    main()
