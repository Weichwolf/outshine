"""Check native canopy colour footprints against independent area integration."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from ground_canopy_native import ROOT, execute, optical_controls
from ground_detail import noise2
from ground_litter_footprint import visibility
from ground_litter_reference import DTYPE


def cases(scale, row):
    rng = np.random.default_rng(451)
    points = rng.uniform(-4, 4, (8, 2))
    probes, groups = [], []
    for width in (0.005, 0.02, 0.1, 0.4, 0.9, 1.3, 1.9, 2.1):
        for angle in (0, 17, 45, 83):
            direction = np.array([np.cos(np.deg2rad(angle)), np.sin(np.deg2rad(angle))])
            normal = np.array([-direction[1], direction[0]])
            for aspect in (0.03, 0.2, 1.0):
                for shear in (0, 0.6):
                    dx = width * direction
                    dy = width * aspect * (normal + shear * direction)
                    start = sum(len(part) for part in probes)
                    value = np.zeros(len(points), dtype=DTYPE)
                    value["point_dx"][:, :2] = points * scale
                    value["point_dx"][:, 2:] = dx * scale
                    value["dy"] = dy * scale
                    value["which"], value["mode"] = row, 1
                    probes.append(value)
                    groups.append(
                        {
                            "start": start,
                            "count": len(points),
                            "width": width,
                            "angle": angle,
                            "aspect": aspect,
                            "shear": shear,
                        }
                    )
    original = np.concatenate(probes)
    swapped = original.copy()
    swapped["point_dx"][:, 2:] = original["dy"]
    swapped["dy"] = original["point_dx"][:, 2:]
    reversed_axes = original.copy()
    reversed_axes["point_dx"][:, 2:] *= -1
    reversed_axes["dy"] *= -1
    motion = np.zeros(256, dtype=DTYPE)
    motion["point_dx"][:, :2] = (
        points[0] + np.arange(256)[:, None] * [0.003, 0.001]
    ) * scale
    motion["point_dx"][:, 2:] = np.array([0.4, 0.17]) * scale
    motion["dy"] = np.array([0.09, 0.08]) * scale
    motion["which"], motion["mode"] = row, 1
    return (
        np.concatenate([original, swapped, reversed_axes, motion]),
        groups,
        len(original),
    )


def reference(probes, scale, samples):
    at = probes["point_dx"][:, :2].astype(float) / scale
    dx = probes["point_dx"][:, 2:].astype(float) / scale
    dy = probes["dy"].astype(float) / scale
    width = np.maximum(np.linalg.norm(dx, axis=1), np.linalg.norm(dy, axis=1))
    active = width < 2
    total = np.zeros(int(active.sum()))
    points, step_x, step_y = at[active], dx[active], dy[active]
    nodes = (np.arange(samples) + 0.5) / samples - 0.5
    for y in nodes:
        for x in nodes:
            total += noise2(points + x * step_x + y * step_y)
    result = np.zeros(len(probes))
    result[active] = (total / samples**2 - 0.5) * visibility(width[active])
    return result


def srgb(value):
    return np.where(
        value <= 0.0031308, 12.92 * value, 1.055 * value ** (1 / 2.4) - 0.055
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--quadrature", type=int, choices=(2, 3), default=3)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    palette = np.fromfile(args.palette, dtype="<f4")
    rows, layers = int(palette[:1].view("<u4")[0]), int(palette[3:4].view("<u4")[0])
    tail = layers + 8 * (rows + 1)
    active = np.flatnonzero(palette[tail:].reshape(-1, 24)[:rows, 19] > 0)
    controls, row = optical_controls(palette, active, rows, tail)
    unit = controls["mixture-optics"]
    scale = float(palette[layers + row * 8 + 6])
    fraction = float(palette[tail + row * 24 + 18])
    contrast = float(palette[4 * (rows + 2) + rows + 1 + row * 8 + 4])
    spread = 2 * min(fraction, 1 - fraction) * contrast
    probes, groups, count = cases(scale, row)
    probes.tofile(output / "probes.bin")
    unit.tofile(output / "mixture.palette.bin")
    outputs, commands = execute(
        args.binary.resolve(),
        output,
        ["mixture"],
        rows,
        [f"-DGROUND_LITTER_GAUSS_POINTS={args.quadrature}"],
    )
    actual = outputs["mixture"][:, 0]
    chosen = np.concatenate([probes[:count], probes[3 * count :]])
    fine = fraction + spread * reference(chosen, scale, 64)
    coarse = fraction + spread * reference(chosen, scale, 32)
    baseline = np.concatenate([actual[:count], actual[3 * count :]])
    convergence = float(np.max(np.abs(srgb(fine) - srgb(coarse))))
    error = np.abs(srgb(baseline) - srgb(fine))
    symmetry = max(
        float(np.max(np.abs(actual[count : 2 * count] - actual[:count]))),
        float(np.max(np.abs(actual[2 * count : 3 * count] - actual[:count]))),
    )
    motion_error = np.diff(srgb(baseline[count:])) - np.diff(srgb(fine[count:]))
    centres = chosen["point_dx"][:, :2].astype(float) / scale
    dx = chosen["point_dx"][:, 2:].astype(float) / scale
    dy = chosen["dy"].astype(float) / scale
    widths = np.maximum(np.linalg.norm(dx, axis=1), np.linalg.norm(dy, axis=1))
    point = fraction + spread * (noise2(centres) - 0.5) * visibility(widths)
    alternatives = {
        "unfiltered_point_max_display_lsb": 255
        * float(np.max(np.abs(srgb(point) - srgb(fine)))),
        "uniform_mean_max_display_lsb": 255
        * float(np.max(np.abs(srgb(fraction) - srgb(fine)))),
    }
    report = {
        "quadrature_samples": args.quadrature**2,
        "probes": len(probes),
        "groups": groups,
        "main_probes": count,
        "commands": commands,
        "palette_sha256": hashlib.sha256(unit.tobytes()).hexdigest(),
        "shader_sha256": hashlib.sha256(
            (output / "canopy-probe.spv").read_bytes()
        ).hexdigest(),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "reference": "64x64 midpoint over actual float32 parallelogram, checked at32x32",
        "display": "neutral albedo sRGB transform only, no runtime tone/lighting claim",
        "convergence_display_lsb": 255 * convergence,
        "max_error_display_lsb": 255 * float(error.max()),
        "rms_error_display_lsb": 255 * float(np.sqrt(np.mean(error**2))),
        "symmetry_max_linear": symmetry,
        "motion_delta_error_display_lsb": 255 * float(np.max(np.abs(motion_error))),
        "threshold_display_lsb": 1.0,
        "negative_controls": alternatives,
    }
    np.savez_compressed(
        output / "reference.npz",
        actual=baseline,
        reference=fine,
        coarse=coarse,
        error=error,
        motion_error=motion_error,
    )
    (output / "measurement.json").write_text(json.dumps(report, indent=2) + "\n")
    summary = {
        key: value for key, value in report.items() if key not in ("groups", "commands")
    }
    assert min(alternatives.values()) > 1.0, summary
    assert convergence < 0.1 / 255, summary
    assert symmetry < 16 * np.finfo(np.float32).eps, summary
    assert error.max() <= 1 / 255, summary
    assert np.max(np.abs(motion_error)) <= 1 / 255, summary
    report["status"] = "passed"
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: value
                for key, value in report.items()
                if key not in ("groups", "commands")
            }
        )
    )


if __name__ == "__main__":
    main()
