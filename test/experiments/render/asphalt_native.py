"""Execute asphalt kernels in the retained common native 32-byte GPU consumer."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from asphalt_field import (
    ALBEDO,
    ALBEDO_AMPLITUDE,
    ROUGHNESS,
    ROUGHNESS_AMPLITUDE,
    filtered,
    point_field,
)
from asphalt_filter import cases

ROOT = Path(__file__).resolve().parents[3]
PROBE = np.dtype(
    [("point_dx", "<f4", 4), ("dy", "<f4", 2), ("which", "<i4"), ("mode", "<u4")]
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, output, name):
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=60, check=False
    )
    log = result.stdout + result.stderr
    (output / f"{name}.log").write_text(log)
    if result.returncode or "Validation Error" in log or "VUID-" in log:
        raise RuntimeError(
            f"native command failed: {name}: {result.returncode}: {log[-2000:]}"
        )
    return log


def inputs(output, repeats):
    rows, expected = [], [[], []]
    for _, points, dx, dy in cases():
        batch = np.zeros(len(points), dtype=PROBE)
        batch["point_dx"][:, :2] = points
        batch["point_dx"][:, 2:] = dx
        batch["dy"] = dy
        # Parity uses the actual float32 inputs, independently evaluated in float64.
        at = batch["point_dx"][:, :2].astype(float)
        step_x = batch["point_dx"][0, 2:].astype(float)
        step_y = batch["dy"][0].astype(float)
        expected[0].append(point_field(at, radius=1))
        expected[1].append(filtered(at, step_x, step_y, "oriented"))
        rows.append(batch)
    probes = np.tile(np.concatenate(rows), repeats)
    palette = np.zeros(320, dtype="<f4")
    palette.view("<u4")[0] = 14
    palette[4:8] = [*ALBEDO, ROUGHNESS]
    palette[8:10] = [ALBEDO_AMPLITUDE, ROUGHNESS_AMPLITUDE]
    palette.tofile(output / "asphalt.palette")
    return probes, [np.tile(np.concatenate(e), repeats) for e in expected]


def capture(binary, output, probes, predicted, method, timed, label):
    probes["which"] = method
    input_path = output / f"probes-{method}.bin"
    probes.tofile(input_path)
    target = output / f"{label}-{method}.f32"
    command = [
        str(binary),
        str(output / "asphalt.spv"),
        str(output / "asphalt.palette"),
        str(input_path),
        str(target),
        "13",
    ]
    if timed:
        command.append("--gpu-timing")
    log = run(command, output, f"{label}-{method}")
    actual = np.fromfile(target, dtype="<f4").reshape(-1, 8)
    if len(actual) != len(probes) or not np.all(np.isfinite(actual)):
        raise ValueError("native output shape or finite-value failure")
    error = float(np.max(np.abs(actual[:, 4] - predicted)))
    if error > 3e-6:
        raise ValueError(f"native asphalt field parity failed: {error}")
    palette = np.fromfile(output / "asphalt.palette", dtype="<f4").astype(float)
    colour = palette[4:7] + palette[8] * predicted[:, None]
    roughness = palette[7] + palette[9] * predicted
    np.testing.assert_allclose(actual[:, :3], colour, rtol=0, atol=3e-7)
    np.testing.assert_allclose(actual[:, 3], roughness, rtol=0, atol=3e-7)
    receipt = {
        "command": command,
        "exit": 0,
        "count": len(probes),
        "field_max_absolute_error": error,
        "output_sha256": sha(target),
        "input_sha256": sha(input_path),
    }
    if timed:
        match = re.search(
            r"profile_available=1 profile_dispatches=(\d+) profile_sum_ms=([\d.e+-]+)",
            log,
        )
        if not match or int(match[1]) != 32:
            raise ValueError("actual dispatch timestamps missing")
        receipt["dispatches"] = 32
        receipt["gpu_dispatch_sum_ms"] = float(match[2])
    return actual, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.repeats < 1 or args.repeats > 64:
        parser.error("repeat count must be1..64")
    args.output = args.output.resolve()
    args.binary = args.binary.resolve()
    if args.output.is_relative_to(ROOT) or not args.output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("output must be outside the checkout in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    command = [
        "nice",
        "-n",
        "10",
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
        "-Itest/experiments/render",
        "test/experiments/render/asphalt_probe.comp",
        "-o",
        str(args.output / "asphalt.spv"),
    ]
    run(command, args.output, "compile")
    probes, expected = inputs(args.output, args.repeats)
    receipts, plain = [], {}
    for method in (0, 1):
        plain[method], receipt = capture(
            args.binary, args.output, probes, expected[method], method, False, "plain"
        )
        receipts.append(receipt)
    if args.profile:
        for label, methods in (("forward", (0, 1)), ("reverse", (1, 0))):
            for method in methods:
                actual, receipt = capture(
                    args.binary,
                    args.output,
                    probes,
                    expected[method],
                    method,
                    True,
                    label,
                )
                np.testing.assert_array_equal(actual, plain[method])
                if actual.tobytes() != plain[method].tobytes():
                    raise ValueError("timed output bytes differ from plain output")
                path = args.output / f"{label}-{method}.f32"
                path.unlink()
                path.hardlink_to(args.output / f"plain-{method}.f32")
                receipts.append(receipt)
    report = {
        "scope": "Native component probe; no production/whole-frame/Place proof",
        "binary_sha256": sha(args.binary),
        "probes": len(probes),
        "input_bytes": probes.nbytes,
        "runs": receipts,
        "source_sha256": {
            p.name: sha(p)
            for p in (
                Path(__file__),
                Path(__file__).with_name("asphalt_field.glsl"),
                Path(__file__).with_name("asphalt_probe.comp"),
                Path(__file__).with_name("asphalt_field.py"),
                Path(__file__).with_name("asphalt_filter.py"),
            )
        },
    }
    (args.output / "native.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
