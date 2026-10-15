"""Native joint facade coating with explicit, camera-independent metric inputs."""

import argparse
import hashlib
import itertools
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from facade_coating import controls
from facade_footprint import (
    INSET,
    MULLION_HIGH,
    MULLION_LOW,
    ROOT,
    box_mask,
    native_profiles,
    prepare,
)
from ground_litter_native import capture
from outshine_ref.buildings.facades import interval_area
from rock_direction import noise


def visibility(metric):
    size = (
        np.maximum(
            np.linalg.norm(metric[:, 1], axis=1), np.linalg.norm(metric[:, 2], axis=1)
        )
        / 1.5
    )
    t = np.clip(size - 1, 0, 1)
    return 1 - t * t * (3 - 2 * t)


def metric_inputs(values):
    profiles = native_profiles()
    metric = np.zeros((len(values), 3, 3))
    for style in np.unique(values[:, 6]).astype(int):
        take = values[:, 6] == style
        plane = values[take, :6].reshape(-1, 3, 2) * profiles[style, :2]
        metric[take, :, 0] = plane[:, :, 0] * np.cos(0.43)
        metric[take, :, 1] = plane[:, :, 1]
        metric[take, :, 2] = plane[:, :, 0] * np.sin(0.43)
    metric[:, 0] += [0.31, 2.73, -4.27]
    return metric.astype(np.float32).astype(float)


def fixtures():
    points, dx, dy = controls()
    profiles = native_profiles()
    values = []
    for style in (1, 8):
        values.append(
            np.column_stack(
                [
                    points / profiles[style, :2],
                    dx / profiles[style, :2],
                    dy / profiles[style, :2],
                    np.full(len(points), style),
                ]
            )
        )
    tracks = []
    for style in (1, 8):
        for track in ("rotation", "translation", "growth"):
            begin = sum(len(v) for v in values)
            rows = []
            for step in range(91):
                t = step / 90
                angle = t * np.pi / 2 if track == "rotation" else 0.7
                size = 0.01 * 350**t if track == "growth" else 0.3
                u = size * np.array([np.cos(angle), np.sin(angle)])
                v = size * np.array([-np.sin(angle), np.cos(angle)])
                for point in np.random.default_rng(278).uniform(-4, 4, (4, 2)):
                    p = point + (
                        np.array([1.1, -0.7]) * t if track == "translation" else 0
                    )
                    rows.append(
                        [
                            *(p / profiles[style, :2]),
                            *(u / profiles[style, :2]),
                            *(v / profiles[style, :2]),
                            style,
                        ]
                    )
            values.append(np.array(rows))
            tracks.append(
                {
                    "style": style,
                    "track": track,
                    "begin": begin,
                    "steps": 91,
                    "points": 4,
                }
            )
    values = np.concatenate(values).astype(np.float32).astype(float)
    metric = metric_inputs(values)
    return values, metric, tracks


def regions(values, per_m, x=0.0, y=0.0, count=1, point=False):
    result = np.zeros((len(values), 3))
    at = np.remainder(values[:, :2], 1) + x * values[:, 2:4] + y * values[:, 4:6]
    width = (np.abs(values[:, 2:4]) + np.abs(values[:, 4:6])) / count
    profiles = native_profiles()
    for style in np.unique(values[:, 6]).astype(int):
        take = values[:, 6] == style
        low, high = (1 - profiles[style, 2:]) / 2, (1 + profiles[style, 2:]) / 2
        if point:
            phase = np.remainder(at[take], 1)
            result[take, 0] = np.all((phase >= low) & (phase < high), axis=1)
        else:
            result[take, 0] = box_mask(at[take], width[take], low, high)
        if min(profiles[style, 2:]) > 2 * INSET and not point:
            result[take, 1] = box_mask(
                at[take],
                width[take],
                low + INSET,
                np.array([MULLION_LOW, high[1] - INSET]),
            )
            result[take, 1] += box_mask(
                at[take],
                width[take],
                np.array([MULLION_HIGH, low[1] + INSET]),
                high - INSET,
            )
        fraction = np.minimum(0.16 * per_m[take], 1 - profiles[style, 3])
        if point:
            result[take, 2] = (
                np.abs(np.remainder(at[take, 1] + 0.5, 1) - 0.5) < fraction / 2
            )
        else:
            span = np.maximum(width[take, 1], 0.0001)
            phase = at[take, 1] + 0.5
            result[take, 2] = (
                interval_area(phase - span / 2, phase + span / 2, fraction) / span
            )
    return result


def parity(values, metric, per_m):
    result = np.zeros((len(values), 4))
    for x, y in itertools.product((-1 / 3, 0.0, 1 / 3), repeat=2):
        masks = regions(values, per_m, x, y, 3)
        field = noise((metric[:, 0] + x * metric[:, 1] + y * metric[:, 2]) / 1.5) - 0.5
        result[:, :3] += masks / 9
        result[:, 3] += field * np.maximum(1 - masks[:, 0] - 0.35 * masks[:, 2], 0) / 9
    axial = ((values[:, 3] == 0) & (values[:, 4] == 0)) | (
        (values[:, 2] == 0) & (values[:, 5] == 0)
    )
    result[axial, :3] = regions(values[axial], per_m[axial])
    result[:, :3] = np.clip(result[:, :3], 0, 1)
    result[:, 1] = np.minimum(result[:, 1], result[:, 0])
    result[:, 2] = np.minimum(result[:, 2], 1 - result[:, 0])
    bound = 0.5 * np.maximum(1 - result[:, 0] - 0.35 * result[:, 2], 0)
    result[:, 3] = np.clip(result[:, 3], -bound, bound) * visibility(metric)
    return result


def area(values, metric, per_m, side):
    total = np.zeros(len(values))
    for x, y in itertools.product((np.arange(side) + 0.5) / side - 0.5, repeat=2):
        masks = regions(values, per_m, x, y, point=True)
        field = noise((metric[:, 0] + x * metric[:, 1] + y * metric[:, 2]) / 1.5) - 0.5
        total += field * np.maximum(1 - masks[:, 0] - 0.35 * masks[:, 2], 0) / side**2
    return total * visibility(metric)


def prepare_input(output, values, metric, per_m):
    prepare(output, values)
    records = np.fromfile(
        output / "probes.bin",
        dtype=[("data", "<f4", 6), ("which", "<i4"), ("mode", "<u4")],
    )
    records["mode"] = per_m.astype("<f4").view("<u4")
    records.tofile(output / "probes.bin")
    original = np.fromfile(output / "palette.f32", dtype="<f4")
    header = (ROOT / "src/generators/building/BuildingMaterials.h").read_text()
    colour = np.array(
        [
            float(v.replace("f", ""))
            for v in re.search(r"Colour\{\{([^}]+)", header)[1].split(",")
        ]
    )
    original[4:8] = [*colour, 0.9]
    extra = np.zeros((len(values), 3, 4), dtype="<f4")
    extra[:, :, :3] = metric
    payload = np.concatenate(
        [original, np.zeros(320 - len(original), dtype="<f4"), extra.ravel()]
    )
    payload.tofile(output / "native.palette")
    return colour


def controls_native(output, binary):
    output.mkdir(exist_ok=True)
    values = np.zeros((24, 7))
    values[:, :2] = [0.25, 0.25]
    values[:, 6] = np.tile([0, 1, 7, 8], 6)
    values[:, 2:6] = [0.03, 0.01, -0.01, 0.03]
    metric = metric_inputs(values)
    metric[-8:, 1:] = [[[3, 0, 0], [0, 3, 0]]]
    per_m = np.full(len(values), float(np.float32(1 / 2.9)))
    prepare_input(output, values, metric, per_m)
    shaders(output)
    original = np.fromfile(output / "native.palette", dtype="<f4")
    result = []
    for label, colour in [
        ("native", original[4:7].copy()),
        ("black", np.zeros(3)),
        ("white", np.ones(3)),
        ("near_limits", np.array([0.9999, 0.0001, 0.5])),
    ]:
        palette = original.copy()
        palette[4:7] = colour
        palette.tofile(output / f"{label}.palette")
        old, new = [
            capture(binary, output, mode, label, len(values), 13) for mode in (0, 1)
        ]
        assert np.min(new[:, :3]) >= -1e-7 and np.max(new[:, :3]) <= 1 + 1e-7
        np.testing.assert_array_equal(new[-8:, 7], np.zeros(8))
        np.testing.assert_allclose(
            old[-8:, :4], new[-8:, :4], atol=8 * np.finfo(np.float32).eps, rtol=0
        )
        glazing = values[:, 6] == 7
        np.testing.assert_array_equal(
            new[glazing, 7], np.zeros(np.count_nonzero(glazing))
        )
        if label in ("black", "white"):
            np.testing.assert_allclose(
                old[:, :4], new[:, :4], atol=8 * np.finfo(np.float32).eps, rtol=0
            )
        result.append(
            {"label": label, "bounded": True, "far_mean": True, "full_glazing": True}
        )
    return result


def shaders(output):
    for coated in (0, 1):
        command = [
            "nice",
            "-n",
            "10",
            os.environ.get("GLSLANG", "glslangValidator"),
            "-V",
            "-Isrc",
            "-Isrc/render/shaders",
            "-Isrc/content/shade",
            "-Itest/experiments/render",
            f"-DFACADE_COATED={coated}",
            "test/experiments/render/facade_coating_probe.comp",
            "-o",
            str(output / f"filter-{coated}.spv"),
        ]
        p = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, check=False
        )
        (output / f"compile-{coated}.log").write_text(p.stdout + p.stderr)
        assert p.returncode == 0, p.stderr + p.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    if args.reference_only:
        data = np.load(output / "cases.npz")
        values, metric, per_m = data["values"], data["metric"], data["per_m"]
        np.savez_compressed(
            output / "reference.npz",
            parity=parity(values, metric, per_m),
            area64=area(values, metric, per_m, 64),
            area128=area(values, metric, per_m, 128),
        )
        return
    if not args.binary:
        parser.error("--binary is required for native validation")
    values, metric, tracks = fixtures()
    per_m = (
        (1 / native_profiles()[values[:, 6].astype(int), 1])
        .astype(np.float32)
        .astype(float)
    )
    colour = prepare_input(output, values, metric, per_m)
    np.savez_compressed(output / "cases.npz", values=values, metric=metric, per_m=per_m)
    command = [
        "nice",
        "-n",
        "10",
        sys.executable,
        str(Path(__file__).resolve()),
        "--output",
        str(output),
        "--reference-only",
    ]
    shaders(output)
    (output / "reference.npz").unlink(missing_ok=True)
    subprocess.run(command, cwd=ROOT, check=True)
    reference = np.load(output / "reference.npz")
    old, new = [
        capture(args.binary.resolve(), output, mode, "native", len(values), 13)
        for mode in (0, 1)
    ]
    np.testing.assert_allclose(new[:, 4:], reference["parity"], rtol=0, atol=2e-4)
    np.testing.assert_allclose(old[:, :4][:, 3], new[:, :4][:, 3], rtol=0, atol=2e-5)
    np.testing.assert_allclose(old[:, 4:7], new[:, 4:7], rtol=0, atol=2e-5)
    expected_colour = (
        old[:, :3]
        + np.minimum(0.16 * colour, 2 * np.minimum(colour, 1 - colour))
        * reference["parity"][:, 3:4]
    )
    np.testing.assert_allclose(new[:, :3], expected_colour, rtol=0, atol=2e-4)
    motion = []
    for track in tracks:
        start, end = track["begin"], track["begin"] + track["steps"] * track["points"]
        residual = (new[:, 7] - reference["area128"])[start:end].reshape(
            track["steps"], track["points"]
        )
        motion.append(
            dict(
                **track,
                step_error_rms=float(np.sqrt(np.mean(np.diff(residual, axis=0) ** 2))),
            )
        )
    checks = controls_native(output / "controls", args.binary.resolve())
    report = {
        "cases": len(values),
        "motion": motion,
        "controls": checks,
        "gpu_executed": True,
        "parity_max": float(np.max(np.abs(new[:, 4:] - reference["parity"]))),
        "weighted_field_area_error": float(
            np.max(np.abs(new[:, 7] - reference["area128"]))
        ),
        "area_refinement_change": float(
            np.max(np.abs(reference["area64"] - reference["area128"]))
        ),
        "measured_colour_change_max": float(np.max(np.abs(new[:, :3] - old[:, :3]))),
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__),
                ROOT / "test/experiments/render/facade_coating_kernel.glsl",
                ROOT / "test/experiments/render/facade_coating_probe.comp",
            ]
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
