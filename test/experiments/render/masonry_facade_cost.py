"""Compare production coating and coating+masonry using public dispatch timestamps."""

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
from facade_footprint import native_profiles
from masonry_native import amplitude, source_colour


def prepare(output, count):
    rng = np.random.default_rng(306)
    profiles = native_profiles()
    styles = np.array([0, 1, 8])[np.arange(count) % 3]
    physical = profiles[styles, :2]
    points = rng.uniform(-30, 30, (count, 2))
    size = np.array([0.0005, 0.005, 0.03, 0.15, 0.7, 3, 20])[np.arange(count) % 7]
    angle = rng.uniform(0, np.pi, count)
    dx = size[:, None] * np.column_stack([np.cos(angle), np.sin(angle)])
    dy = size[:, None] * np.column_stack([-np.sin(angle), np.cos(angle)])
    dy += dx * 0.37
    styles[-512:] = 0
    physical[-512:] = profiles[0, :2]
    dx[-512:] = [20, 0]
    dy[-512:] = [0, 20.48]
    records = np.zeros(
        count, dtype=[("data", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    records["data"] = np.column_stack([points / physical, dx / physical, dy / physical])
    records["which"] = styles
    per_m = np.where(styles == 0, 0, 1 / physical[:, 1]).astype("<f4")
    records["mode"] = per_m.view("<u4")
    records.tofile(output / "probes.bin")
    original = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    assert len(original) <= 320
    palette = np.zeros(320 + 20 * count, dtype="<f4")
    palette[: len(original)] = original
    palette[4:8] = [*source_colour(), 0.9]
    metric = palette[320:].reshape(count, 20)
    for column, values in ((0, points), (4, dx), (8, dy)):
        metric[:, column : column + 2] = values
    metric[:, 12:18] = np.column_stack([points, dx, dy])
    palette.tofile(output / "native.palette")
    for selected in (0, 1):
        run(
            [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/content/shade",
                f"-DFACADE_MASONRY={selected}",
                "test/experiments/render/masonry_facade_probe.comp",
                "-o",
                str(output / f"filter-{selected}.spv"),
            ],
            output,
            f"compile-{selected}",
        )
    return int(original[:1].view("<u4")[0]) - 1


def capture(binary, output, selected, unmapped, timed, pass_index):
    label = f"{pass_index}-{selected}-{'timed' if timed else 'plain'}"
    target = output / f"{label}.f32"
    command = [
        str(binary),
        str(output / f"filter-{selected}.spv"),
        str(output / "native.palette"),
        str(output / "probes.bin"),
        str(target),
        str(unmapped),
        *(["--gpu-timing"] if timed else []),
    ]
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=60, check=False
    )
    log = result.stdout + result.stderr
    (output / f"{label}.log").write_text(log)
    assert (
        result.returncode == 0 and "VUID-" not in log and "Validation Error" not in log
    ), log
    values = np.fromfile(target, dtype="<f4").reshape(-1, 8)
    assert np.isfinite(values).all()
    measurement = re.search(
        r"profile_available=1 profile_dispatches=(\d+) profile_sum_ms=([\d.e+-]+)", log
    )
    if timed:
        assert measurement and int(measurement[1]) == 32, log
    return values, {
        "command": command,
        "exit": result.returncode,
        "device": re.search(r"device=(.+) probes=", log)[1],
        "dispatches": 32 if timed else 1,
        "dispatch_sum_ms": float(measurement[2]) if measurement else None,
        "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) and not output.is_relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    count = 115200
    unmapped = prepare(output, count)
    baseline, records = [], []
    for selected in (0, 1):
        values, record = capture(
            args.binary.resolve(), output, selected, unmapped, False, 0
        )
        assert len(values) == count
        baseline.append(values)
        records.append(record)
    difference = baseline[1][:, :4] - baseline[0][:, :4]
    field = baseline[1][:, 7]
    np.testing.assert_allclose(
        difference[:, :3],
        field[:, None] * amplitude(source_colour()),
        rtol=0,
        atol=2e-6,
    )
    np.testing.assert_allclose(difference[:, 3], field * 0.04, rtol=0, atol=2e-6)
    np.testing.assert_array_equal(baseline[0][:, 4:7], baseline[1][:, 4:7])
    far = np.arange(count) >= count - 512
    np.testing.assert_allclose(field[far], 0, rtol=0, atol=2e-6)
    for pass_index, order in enumerate(((0, 1), (1, 0)), start=1):
        for selected in order:
            values, record = capture(
                args.binary.resolve(), output, selected, unmapped, True, pass_index
            )
            np.testing.assert_array_equal(values, baseline[selected])
            records.append(record)
    report = {
        "scope": "Production material components; sum of 32 dispatch ranges. No copies, frame, Metal or A18 claim.",
        "probes": count,
        "far_mean_controls": int(far.sum()),
        "records": records,
        "selected_work": {
            "coating_noise_evaluations": 9,
            "additional_joint_subcells": 16,
            "additional_texture_or_hash_samples": 0,
            "new_production_bindings": 0,
        },
        "output_invariant_under_timestamps": True,
        "load": Path("/proc/loadavg").read_text().strip()
        if Path("/proc/loadavg").exists()
        else None,
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
    }
    (output / "cost.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "probes": count,
                "far_mean_controls": report["far_mean_controls"],
                "dispatch_sum_ms": [r["dispatch_sum_ms"] for r in records[2:]],
            }
        )
    )


if __name__ == "__main__":
    main()
