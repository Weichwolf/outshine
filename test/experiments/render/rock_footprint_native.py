"""Execute the rock footprint candidates through the existing native GPU consumer."""

import argparse
import hashlib
import json
import os
import re
import resource
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT, run
from ground_gradients import surface_gradient
from ground_litter_native import capture
from rock_footprint import cases, field, normal, reference

METHODS = {"legacy": 0, "mapped": 1, "linear": 2, "production": 3}


def profile(binary, output, metrics, samples, unmapped):
    destination = output / "cost"
    destination.mkdir(exist_ok=True)
    indices = np.arange(115200) % len(metrics)
    source = np.fromfile(output / "native.palette", dtype="<f4")
    palette = np.zeros(320 + 12 * len(indices), dtype="<f4")
    palette[:320] = source[:320]
    palette[320:] = metrics[indices].astype("<f4").ravel()
    palette.tofile(destination / "native.palette")
    probes = np.zeros(
        len(indices), dtype=[("values", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(indices))
    probes.tofile(destination / "probes.bin")
    records = []
    for pass_index, order in enumerate(
        (("legacy", "production"), ("production", "legacy"))
    ):
        for mode in order:
            name = f"{pass_index}-{mode}"
            target = destination / (name + ".f32")
            command = [
                str(binary),
                str(output / f"filter-{METHODS[mode]}.spv"),
                str(destination / "native.palette"),
                str(destination / "probes.bin"),
                str(target),
                str(unmapped),
                "--gpu-timing",
            ]
            log = run(command, destination, name)
            assert "VUID-" not in log and "Validation Error" not in log
            timed = re.search(
                r"profile_available=1 profile_dispatches=(\d+) profile_sum_ms=([\d.e+-]+)",
                log,
            )
            assert timed and int(timed[1]) == 32, log
            values = np.fromfile(target, dtype="<f4").reshape(-1, 8)
            np.testing.assert_array_equal(values, samples[mode][indices])
            records.append(
                {
                    "mode": mode,
                    "command": command,
                    "exit": 0,
                    "dispatch_sum_ms": float(timed[2]),
                    "dispatches": 32,
                    "device": re.search(r"device=(.+) probes=", log)[1],
                    "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                }
            )
    processes = subprocess.run(
        ["ps", "-eo", "comm,pid,pcpu,pmem"], capture_output=True, text=True, check=True
    )
    (destination / "process-load.txt").write_text(processes.stdout)
    report = {
        "records": records,
        "probes": len(indices),
        "same_output_as_plain": True,
        "buffer_requests": 5,
        "buffer_bytes": 2 * palette.nbytes + 4 * probes.nbytes,
        "copy_bytes": palette.nbytes + 2 * probes.nbytes,
        "load_average": os.getloadavg(),
        "scope": "Sum of32 public native dispatch ranges. Copies/barriers/frame/driver peak excluded.",
    }
    (destination / "cost.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def prepare(output, baseline):
    palette = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    rows, rock = [int(palette[i : i + 1].view("<u4")[0]) for i in (0, 1)]
    at = 4 * (rows + 2) + rows + 1 + rock * 8
    material = {
        "heightAmplitudeM": float(palette[at + 1]),
        "detailScaleM": palette[at + 2 : at + 4].astype(float).tolist(),
        "albedo": palette[4 + 4 * rock : 7 + 4 * rock].astype(float).tolist(),
    }
    contrast = float(palette[at + 4])
    fixtures = list(cases())
    metrics = np.zeros((len(fixtures), 12), dtype="<f4")
    for index, (_, point, dx, dy) in enumerate(fixtures):
        metrics[index, :3], metrics[index, 4:7], metrics[index, 8:11] = point, dx, dy
    combined = np.zeros(320 + metrics.size, dtype="<f4")
    combined[: len(palette)], combined[320:] = palette, metrics.ravel()
    combined.tofile(output / "native.palette")
    probes = np.zeros(
        len(fixtures), dtype=[("values", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(fixtures))
    probes.tofile(output / "probes.bin")
    for mode, method in METHODS.items():
        include = baseline if mode == "legacy" else ROOT
        run(
            [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "-I" + str(include / "src/render/shaders"),
                f"-DROCK_PIXEL_METHOD={method}",
                "test/experiments/render/rock_footprint_probe.comp",
                "-o",
                str(output / f"filter-{method}.spv"),
            ],
            output,
            "compile-" + mode,
        )
    return fixtures, metrics.astype(float), material, contrast, rows - 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline-checkout", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) and not output.is_relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    fixtures, metrics, material, contrast, unmapped = prepare(
        output, args.baseline_checkout.resolve()
    )
    samples = {
        mode: capture(
            args.binary.resolve(), output, method, "native", len(fixtures), unmapped
        )
        for mode, method in METHODS.items()
    }
    palette = np.fromfile(output / "native.palette", dtype="<f4")
    count_rows, rock = [int(palette[i : i + 1].view("<u4")[0]) for i in (0, 1)]
    at_recipe = 4 * (count_rows + 2) + count_rows + 1 + 8 * rock
    controls = []
    for name, column in (
        ("disabled-coarse", 2),
        ("disabled-fine", 3),
        ("unresolved", None),
    ):
        changed = palette.copy()
        if column is None:
            data = changed[320:].reshape(-1, 12)
            data[:, 4:7] *= 1e6
            data[:, 8:11] *= 1e6
        else:
            changed[at_recipe + column] = 0
        changed.tofile(output / (name + ".palette"))
        for mode, method in METHODS.items():
            pixels = capture(
                args.binary.resolve(), output, method, name, len(fixtures), unmapped
            )
            np.testing.assert_array_equal(
                pixels, np.tile([1, 0, 0, 0, 0, 0, 0, 0], (len(fixtures), 1))
            )
            controls.append({"case": name, "mode": mode})
    names = [x[0] for x in fixtures]
    for label in ("macro", "bedding"):
        first = names.index(f"critical-{label}-p1-0.1500")
        second = names.index(f"swapped-{label}")
        for mode in METHODS:
            np.testing.assert_array_equal(
                samples[mode][first, :2], samples[mode][second, :2]
            )
            np.testing.assert_array_equal(
                samples[mode][first, 2:4], samples[mode][second, [3, 2]]
            )
    points, dx, dy = metrics[:, :3], metrics[:, 4:7], metrics[:, 8:11]
    rows = []
    for index, (name, *_) in enumerate(fixtures):
        at, u, v = points[index], dx[index], dy[index]
        expected, convergence = reference(at, u, v, material, contrast)
        reference_normal = normal(expected[2:][None], u, v)[0]
        n = np.cross(u, v)
        n /= np.linalg.norm(n)
        row = {"case": name, "reference": convergence}
        for mode in METHODS:
            actual = samples[mode][index, :4].astype(float)
            cpu, hashes = field(at[None], u[None], v[None], material, contrast, mode)
            np.testing.assert_allclose(actual[:2], cpu[0, :2], rtol=0, atol=2e-4)
            gradient = surface_gradient(n, u, v, actual[2], actual[3], True)
            gpu_normal = normal(gradient[None], u, v)[0]
            cpu_normal = normal(cpu[:, 2:], u, v)[0]
            parity_angle = float(
                np.degrees(np.arccos(np.clip(gpu_normal @ cpu_normal, -1, 1)))
            )
            assert parity_angle < 0.05, (name, mode, parity_angle)
            row[mode] = {
                "rgb_error": float(
                    abs(actual[0] - expected[0]) * max(material["albedo"])
                ),
                "roughness_error": float(abs(actual[1] - expected[1])),
                "normal_error_deg": float(
                    np.degrees(np.arccos(np.clip(gpu_normal @ reference_normal, -1, 1)))
                ),
                "cpu_normal_difference_deg": parity_angle,
                "cpu_scalar_difference": float(np.max(np.abs(actual[:2] - cpu[0, :2]))),
                "corner_hashes": hashes,
            }
        rows.append(row)
    report = {
        "cases": rows,
        "material": material,
        "contrast": contrast,
        "controls": controls,
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "scope": "Real native GPU component with fixed native palette; no complete BRDF/Place/device budget proof.",
    }
    if args.profile:
        report["cost"] = profile(
            args.binary.resolve(), output, metrics, samples, unmapped
        )
    factor = 1 if sys.platform == "darwin" else 1024
    report["parent_peak_rss_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * factor
    )
    report["child_peak_rss_bytes"] = (
        resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss * factor
    )
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cases": len(rows),
                "parity_normal_max_deg": max(
                    r[m]["cpu_normal_difference_deg"] for r in rows for m in METHODS
                ),
            }
        )
    )


if __name__ == "__main__":
    main()
