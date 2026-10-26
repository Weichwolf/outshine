"""Measure actual GGX mixture work, with ideal statistics already resident."""

import argparse
import hashlib
import json
import os
import re
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT, run
from relief_distribution import CAPACITIES, response, response_bounds
from relief_distribution_checks import dots
from relief_shading import ROUGHNESS
from relief_shading_reference import direction

COUNT = 115200
LIGHTS = np.array([direction(angle) for angle in np.linspace(-78, 78, 65)])


def inputs(source, output, capacity, names):
    original = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette = np.zeros(320 + 8 * len(names), dtype="<f4")
    palette[: len(original)] = original
    additions, models = [], []
    offset = len(palette)
    for index, name in enumerate(names):
        with np.load(source / f"{name}-{capacity}.npz") as raw:
            lobes, coverage = raw["lobes"], raw["coverage"]
            mean = raw["mean_gradient_normal"]
        block = np.column_stack([lobes, coverage]).astype("<f4")
        at = 320 + 8 * index
        palette[at : at + 3] = mean
        palette[at + 4 : at + 6] = np.array([offset, len(lobes)], dtype="<u4").view(
            "<f4"
        )
        additions.append(block.ravel())
        offset += block.size
        models.append(
            (
                block[:, :3].astype(float),
                block[:, 3].astype(float),
                palette[at : at + 3].astype(float),
            )
        )
    palette = np.concatenate([palette, *additions])
    palette.tofile(output / f"palette-{capacity}.f32")
    return models, len(palette) * 4


def probes(output, cases):
    index = np.arange(COUNT)
    which = index % cases
    roughness = (index // cases) % 3
    view = (index // (cases * 3)) % 2
    angle = (index // (cases * 6)) % len(LIGHTS)
    values = np.zeros(
        COUNT,
        dtype=[
            ("point", "<f4", 4),
            ("view", "<f4", 2),
            ("which", "<i4"),
            ("mode", "<u4"),
        ],
    )
    values["point"][:, :3] = LIGHTS[angle]
    values["point"][:, 3] = np.array(ROUGHNESS)[roughness]
    values["view"] = np.array([direction(a)[[0, 2]] for a in (0, 50)])[view]
    values["which"] = which
    values.tofile(output / "probes.bin")
    return values, roughness, view, angle


def expected(models, values, roughness, views, angles, mixture):
    predicted = np.empty(len(values))
    lower, upper = np.empty(len(values)), np.empty(len(values))
    for which, (lobes, coverage, mean) in enumerate(models):
        if not mixture:
            lobes, coverage = mean[None], np.ones(1)
        for r in range(3):
            for view in range(2):
                selected = (
                    (values["which"] == which) & (roughness == r) & (views == view)
                )
                lights = LIGHTS.astype("<f4").astype(float)
                v = direction((0, 50)[view])[[0, 2]].astype("<f4").astype(float)
                curve = response(
                    lobes,
                    coverage,
                    float(np.float32(ROUGHNESS[r])),
                    lights,
                    np.array([v[0], 0, v[1]]),
                )
                predicted[selected] = curve[angles[selected]]
                lo, hi = response_bounds(
                    lobes,
                    coverage,
                    float(np.float32(ROUGHNESS[r])),
                    lights,
                    np.array([v[0], 0, v[1]]),
                )
                lower[selected], upper[selected] = (
                    lo[angles[selected]],
                    hi[angles[selected]],
                )
    return predicted, lower, upper


def shader(output, mixture):
    path = output / f"mixture-{mixture}.spv"
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
            f"-DRELIEF_USE_MIXTURE={mixture}",
            "test/experiments/render/relief_distribution_probe.comp",
            "-o",
            str(path),
        ],
        output,
        f"compile-{mixture}",
    )
    return path


def capture(binary, output, method, capacity, mixture, label, timed):
    target = output / f"{label}-{method}.f32"
    command = [
        str(binary),
        str(output / f"mixture-{mixture}.spv"),
        str(output / f"palette-{capacity}.f32"),
        str(output / "probes.bin"),
        str(target),
        "13",
    ]
    if timed:
        command.append("--gpu-timing")
    log = run(command, output, f"{label}-{method}")
    if "Validation Error" in log or "VUID-" in log:
        raise AssertionError("native validation failure")
    actual = np.fromfile(target, dtype="<f4").reshape(-1, 8)
    if len(actual) != COUNT or not np.isfinite(actual).all():
        raise AssertionError("native output shape/nonfinite failure")
    receipt = {
        "method": method,
        "command": command,
        "exit": 0,
        "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
    }
    if timed:
        match = re.search(
            r"profile_available=1 profile_dispatches=(\d+) profile_sum_ms=([\d.e+-]+)",
            log,
        )
        if not match or int(match[1]) != 32:
            raise AssertionError("missing actual native GPU timings")
        receipt["dispatches"] = 32
        receipt["gpu_dispatch_sum_ms"] = float(match[2])
    return target, actual, receipt


def execute(binary, source, output, profile):
    study = json.loads((source / "report.json").read_text())
    names = [c["name"] for c in study["cases"]]
    values, roughness, views, angles = probes(output, len(names))
    shader(output, 0)
    shader(output, 1)
    methods = [("mean", 9, 0)] + [(str(c), c, 1) for c in CAPACITIES]
    reports, models_by_capacity, palettes = [], {}, {}
    for capacity in CAPACITIES:
        models_by_capacity[capacity], palettes[capacity] = inputs(
            source, output, capacity, names
        )
    dot_receipt = dots(binary, output, models_by_capacity, LIGHTS)
    captures = {}
    for method, capacity, mixture in methods:
        target, actual, receipt = capture(
            binary, output, method, capacity, mixture, "plain", False
        )
        captures[method] = target
        models = models_by_capacity[capacity]
        predicted, lower, upper = expected(
            models, values, roughness, views, angles, mixture
        )
        error = np.abs(actual[:, 0] - predicted)
        outside = np.maximum(lower - actual[:, 0], actual[:, 0] - upper)
        if not np.all(outside <= 3e-6 + 1e-4 * np.abs(predicted)):
            raise AssertionError(f"GGX mixture parity failure {method}: {error.max()}")
        np.testing.assert_array_equal(actual[:, 0], actual[:, 1])
        np.testing.assert_array_equal(actual[:, 0], actual[:, 2])
        counts = np.array([len(m[0]) if mixture else 1 for m in models])[
            values["which"]
        ]
        np.testing.assert_array_equal(actual[:, 3], counts)
        receipt.update(
            max_abs_parity_error=float(error.max()),
            max_interval_violation=float(max(outside.max(), 0)),
            max_dot_uncertainty_interval_width=float(np.max(upper - lower)),
            evaluated_lobes=int(counts.sum()),
            max_lobes=int(counts.max()),
            mean_lobes=float(counts.mean()),
            requested_buffers=5,
            requested_buffer_bytes=2 * palettes[capacity] + 4 * values.nbytes,
            upload_mapping_write_bytes=palettes[capacity] + values.nbytes,
            gpu_copy_bytes=palettes[capacity] + 2 * values.nbytes,
            readback_bytes=values.nbytes,
        )
        reports.append(receipt)
        print(
            method,
            "native parity",
            float(error.max()),
            "lobes",
            int(counts.sum()),
            flush=True,
        )
    timed_runs = []
    if profile:
        for order, sequence in enumerate((methods, methods[::-1])):
            for method, capacity, mixture in sequence:
                target, _, receipt = capture(
                    binary, output, method, capacity, mixture, str(order), True
                )
                if target.read_bytes() != captures[method].read_bytes():
                    raise AssertionError("timed/plain capture mismatch")
                timed_runs.append(receipt)
                target.unlink()
    return {
        "scope": "GGX evaluation with ideal resident statistics; no construction or whole-frame cost claim",
        "probes": COUNT,
        "dot_validation": dot_receipt,
        "source_report_sha256": hashlib.sha256(
            (source / "report.json").read_bytes()
        ).hexdigest(),
        "native": reports,
        "gpu_timings": timed_runs,
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = execute(args.binary.resolve(), args.source, args.output, args.profile)
    report["source_sha256"] = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (
            Path(__file__).resolve(),
            Path(__file__).with_name("relief_distribution_probe.comp").resolve(),
            Path(__file__).with_name("relief_distribution.py").resolve(),
            Path(__file__).with_name("relief_distribution_checks.py").resolve(),
            ROOT / "src/render/shaders/metalRoughBrdf.glsl",
        )
    }
    (args.output / "native.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        "PASS", COUNT, "probes per method", len(report["native"]), "methods", flush=True
    )


if __name__ == "__main__":
    main()
