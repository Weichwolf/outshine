"""Compare bounded facade filters on identical native profiles and pixel footprints."""

import argparse
import hashlib
import itertools
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from facade_coating import controls
from outshine_ref.buildings.facades import interval_area
from shapely import Polygon, box

ROOT = Path(__file__).resolve().parents[3]
OPENING_VALUES = (ROOT / "src/content/shade/FacadeOpeningValues.h").read_text()
INSET, MULLION_LOW, MULLION_HIGH = [
    float(re.search(name + r" = ([0-9.]+)f", OPENING_VALUES)[1])
    for name in ("kPaneInset", "kMullionLowU", "kMullionHighU")
]


def native_profiles():
    return np.array(
        [
            list(map(float, line.rstrip(")").split(",")[-4:]))
            for line in (ROOT / "src/content/shade/FacadeProfiles.inc")
            .read_text()
            .splitlines()
        ]
    )


def box_mask(at, width, low, high):
    result = np.ones(len(at))
    for axis in range(2):
        fraction = high[axis] - low[axis]
        phase = at[:, axis] - (low[axis] + high[axis]) / 2 + 0.5
        span = np.maximum(width[:, axis], 0.0001)
        result *= interval_area(phase - span / 2, phase + span / 2, fraction) / span
    return np.clip(result, 0, 1)


def screen_rectangle(at, dx, dy, low, high):
    polygon = box(-0.5, -0.5, 0.5, 0.5)
    for axis in range(2):
        normal = np.array([dx[axis], dy[axis]])
        for direction, offset in [
            (normal, at[axis] - low[axis]),
            (-normal, high[axis] - at[axis]),
        ]:
            radius = np.sum(np.abs(direction)) / 2
            if offset >= radius:
                continue
            if offset <= -radius:
                return 0.0
            squared = direction @ direction
            anchor = -offset * direction / squared
            inward = direction / np.sqrt(squared)
            tangent = np.array([-inward[1], inward[0]])
            polygon = polygon.intersection(
                Polygon(
                    [
                        anchor - 4 * tangent,
                        anchor + 4 * tangent,
                        anchor + 4 * tangent + 4 * inward,
                        anchor - 4 * tangent + 4 * inward,
                    ]
                )
            )
    return polygon.area


def polygon_mask(at, dx, dy, low, high):
    result = []
    for p, u, v in zip(at, dx, dy, strict=True):
        polygon = Polygon(
            [p - (u + v) / 2, p + (u - v) / 2, p + (u + v) / 2, p + (v - u) / 2]
        )
        width = np.abs(u) + np.abs(v)
        begin, end = (
            np.floor(p - width / 2).astype(int),
            np.floor(p + width / 2).astype(int),
        )
        area = 0.0
        for i, j in itertools.product(
            range(begin[0], end[0] + 1), range(begin[1], end[1] + 1)
        ):
            cell = np.array([i, j])
            area += (
                polygon.intersection(box(*(cell + low), *(cell + high))).area
                / polygon.area
                if polygon.area > 1e-14
                else screen_rectangle(p, u, v, cell + low, cell + high)
            )
        result.append(area)
    return np.array(result)


def candidate(at, dx, dy, low, high, kind):
    at = np.remainder(at, 1)
    width = np.abs(dx) + np.abs(dy)
    baseline = box_mask(at, width, low, high)
    if kind == 0:
        return baseline
    if kind == 5:
        regular = ((dx[:, 1] == 0) & (dy[:, 0] == 0)) | (
            (dx[:, 0] == 0) & (dy[:, 1] == 0)
        )
        result = candidate(at, dx, dy, low, high, 3)
        result[regular] = baseline[regular]
        return result
    if kind == 9:
        size = np.max(width, axis=1)
        active = (size < 2) & (np.min(width, axis=1) >= 0.0001)
        result = baseline.copy()
        blend = np.clip(size[active] - 1, 0, 1)
        blend = blend * blend * (3 - 2 * blend)
        result[active] = (
            polygon_mask(at[active], dx[active], dy[active], low, high) * (1 - blend)
            + baseline[active] * blend
        )
        return result
    result = np.zeros(len(at))
    for u, v in itertools.product((np.arange(kind) + 0.5) / kind - 0.5, repeat=2):
        result += box_mask(at + u * dx + v * dy, width / kind, low, high) / kind**2
    return result


def fixtures():
    at, dx, dy = controls()
    records = []
    profiles = native_profiles()
    for style in (1, 4, 8, 9, 10):
        records.append(
            np.column_stack(
                [
                    at / profiles[style, :2],
                    dx / profiles[style, :2],
                    dy / profiles[style, :2],
                    np.full((len(at), 1), style),
                ]
            )
        )
    tracks = []
    for style in (1, 8):
        for track in ("rotation", "translation", "growth", "shear"):
            begin = sum(len(r) for r in records)
            rows = []
            for step in range(91):
                t = step / 90
                angle = t * np.pi / 2 if track == "rotation" else 0.7
                scale = 0.01 * 400**t if track == "growth" else 0.3
                u = scale * np.array([np.cos(angle), np.sin(angle)])
                v = scale * np.array([-np.sin(angle), np.cos(angle)])
                if track == "shear":
                    v += t * 3 * u
                for point in np.random.default_rng(285).uniform(0, 1, (8, 2)):
                    position = point + (
                        t * np.array([1.1, -0.7]) if track == "translation" else 0
                    )
                    rows.append([*position, *u, *v, style])
            records.append(np.array(rows))
            tracks.append(
                {
                    "style": style,
                    "track": track,
                    "begin": begin,
                    "steps": 91,
                    "points": 8,
                }
            )
    special = []
    for style in (1, 4, 7, 8, 9, 10):
        for point in ([0.32, 0.27], [-4.2, 2.1], [128.32, 1024.27]):
            for u, v in [
                ([0.3, 0.4], [0, 0]),
                ([0.3, 0.4], [0.15, 0.2]),
                ([0.3, 0.4], [-0.15, -0.2]),
                ([0.3, 0.4], [-0.4, 0.3]),
                ([16, 0], [0, 32]),
            ]:
                special.append([*point, *u, *v, style])
    records.append(np.array(special))
    values = np.concatenate(records).astype(np.float32).astype(np.float64)
    return values, tracks


def masks(values, kind):
    result = np.zeros((len(values), 2))
    profiles = native_profiles()
    for style in np.unique(values[:, 6]).astype(int):
        take = values[:, 6] == style
        at, dx, dy = values[take, :2], values[take, 2:4], values[take, 4:6]
        low, high = (1 - profiles[style, 2:]) / 2, (1 + profiles[style, 2:]) / 2
        method = polygon_mask if kind == -1 else lambda *args: candidate(*args, kind)
        result[take, 0] = method(at, dx, dy, low, high)
        result[take, 1] = method(
            at, dx, dy, low + INSET, np.array([MULLION_LOW, high[1] - INSET])
        )
        result[take, 1] += method(
            at, dx, dy, np.array([MULLION_HIGH, low[1] + INSET]), high - INSET
        )
    return result


def prepare(output, values):
    records = np.zeros(
        len(values),
        dtype=[
            ("point_dx", "<f4", 4),
            ("dy", "<f4", 2),
            ("which", "<i4"),
            ("mode", "<u4"),
        ],
    )
    assert records.dtype.itemsize == 32
    records["point_dx"], records["dy"], records["which"] = (
        values[:, :4],
        values[:, 4:6],
        values[:, 6],
    )
    records["mode"] = np.float32(1 / 2.9).view("<u4")
    records.tofile(output / "probes.bin")
    palette = ROOT / "test/outshine/data/materials/ground-litter.palette"
    shutil.copyfile(palette, output / "palette.f32")


def capture(output, binary, kind):
    glslang = os.environ.get("GLSLANG", "glslangValidator")
    commands = [
        (
            [
                "nice",
                "-n",
                "10",
                glslang,
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Itest/experiments/render",
                "-Isrc/content/shade",
                f"-DFACADE_FILTER={kind}",
                "test/experiments/render/facade_footprint_probe.comp",
                "-o",
                str(output / f"{kind}.spv"),
            ],
            "compile",
        ),
        (
            [
                str(binary),
                str(output / f"{kind}.spv"),
                str(output / "palette.f32"),
                str(output / "probes.bin"),
                str(output / f"{kind}.f32"),
                "13",
            ],
            "native",
        ),
    ]
    for command, phase in commands:
        if phase == "native":
            (output / f"{kind}.f32").unlink(missing_ok=True)
        p = subprocess.run(
            command, check=False, cwd=ROOT, capture_output=True, text=True, timeout=60
        )
        log = p.stdout + p.stderr
        (output / f"{kind}-{phase}.log").write_text(log)
        assert (
            p.returncode == 0 and "VUID-" not in log and "Validation Error" not in log
        ), (command, log)
    return np.fromfile(output / f"{kind}.f32", dtype="<f4").reshape(-1, 8)


def invariants(output, binary, values):
    output.mkdir(exist_ok=True)
    base = values[:128].copy()
    base[:, :2] = np.round(base[:, :2] * 1024) / 1024
    mirrored, swapped, shifted, axial, point = [base.copy() for _ in range(5)]
    mirrored[:, 2:4] *= -1
    swapped[:, 2:4], swapped[:, 4:6] = base[:, 4:6], base[:, 2:4]
    shifted[:, :2] += [128, 1024]
    axial[:, 2:6] = [0.3, 0, 0, 0.4]
    point[:, 2:6] = 0
    prepare(output, np.concatenate([base, mirrored, swapped, shifted, axial, point]))
    old, new = capture(output, binary, 0), capture(output, binary, 5)
    groups = new.reshape(6, len(base), 8)
    for transformed in groups[1:3]:
        np.testing.assert_allclose(groups[0], transformed, atol=4e-5, rtol=0)
    np.testing.assert_array_equal(groups[0], groups[3])
    np.testing.assert_allclose(
        old[4 * len(base) :],
        new[4 * len(base) :],
        rtol=0,
        atol=8 * np.finfo(np.float32).eps,
    )
    report = {
        "cases": 6 * len(base),
        "sign_and_swap_atol": 4e-5,
        "integer_phase_exact": True,
        "axial_and_zero_derivatives_absolute_budget": float(
            8 * np.finfo(np.float32).eps
        ),
        "axial_and_zero_derivatives_max_difference": float(
            np.max(np.abs(old[4 * len(base) :] - new[4 * len(base) :]))
        ),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    values, tracks = fixtures()
    prepare(output, values)
    reference = masks(values, -1)
    reports = []
    palette = np.fromfile(output / "palette.f32", dtype="<f4")
    for kind in (0, 2, 3, 4, 9, 5):
        expected = masks(values, kind)
        actual = capture(output, args.binary.resolve(), kind)
        assert len(actual) == len(values) and np.isfinite(actual).all()
        np.testing.assert_allclose(actual[:, 4:6], expected, rtol=0, atol=2e-4)
        assert (
            actual[:, 5].min() >= -1e-6 and np.max(actual[:, 5] - actual[:, 4]) <= 2e-5
        )
        expected_band = np.zeros(len(values))
        for style in np.unique(values[:, 6]).astype(int):
            take = values[:, 6] == style
            fraction = min(
                0.16 * float(np.float32(1 / 2.9)), 1 - native_profiles()[style, 3]
            )
            expected_band[take] = candidate(
                values[take, :2] + [0, 0.5],
                values[take, 2:4],
                values[take, 4:6],
                np.array([0, (1 - fraction) / 2]),
                np.array([1, (1 + fraction) / 2]),
                5 if kind == 5 else 0,
            )
        if kind == 5:
            expected_band = np.minimum(expected_band, 1 - expected[:, 0])
            assert np.max(actual[:, 4] + actual[:, 6]) <= 1 + 2e-5
        np.testing.assert_allclose(actual[:, 6], expected_band, atol=2e-4, rtol=0)
        opening, pane = expected.T
        band = np.minimum(expected_band, 1 - opening)
        colour = (1 - opening[:, None]) * palette[4:7] + (opening - pane)[:, None] * [
            0.48,
            0.46,
            0.41,
        ]
        colour += (
            pane[:, None] * [0.025, 0.035, 0.045] - 0.35 * band[:, None] * palette[4:7]
        )
        roughness = (1 - opening) * palette[7] + (opening - pane) * 0.55 + pane * 0.12
        np.testing.assert_allclose(
            actual[:, :4], np.column_stack([colour, roughness]), rtol=0, atol=2e-4
        )
        error = actual[:, 4:6] - reference
        motion = []
        for track in tracks:
            start, end = (
                track["begin"],
                track["begin"] + track["steps"] * track["points"],
            )
            changes = np.diff(
                error[start:end].reshape(track["steps"], track["points"], 2), axis=0
            )
            motion.append(
                dict(**track, step_error_rms=float(np.sqrt(np.mean(changes**2))))
            )
        reports.append(
            {
                "kind": kind,
                "max_area_error": float(np.max(np.abs(error))),
                "rms_area_error": float(np.sqrt(np.mean(error**2))),
                "motion": motion,
                "shader_parity_max": float(np.max(np.abs(actual[:, 4:6] - expected))),
            }
        )
        print({k: v for k, v in reports[-1].items() if k != "motion"}, flush=True)
    np.savez_compressed(output / "cases.npz", values=values, reference=reference)
    identities = invariants(output / "invariants", args.binary.resolve(), values)
    paths = [
        Path(__file__),
        ROOT / "test/experiments/render/facade_area_clip.glsl",
        ROOT / "test/experiments/render/facade_footprint_probe.comp",
        ROOT / "src/content/shade/FacadeProfiles.inc",
        ROOT / "src/content/shade/FacadeOpeningValues.h",
        ROOT / "src/render/shaders/facadeFootprint.glsl",
        ROOT / "src/render/shaders/facadePattern.glsl",
        ROOT / "src/render/shaders/facadeBands.glsl",
        ROOT / "test/experiments/render/facade_coating.py",
        ROOT / "lib/src/outshine_ref/buildings/facades.py",
    ]
    report = {
        "scope": __doc__,
        "cases": len(values),
        "results": reports,
        "gpu_executed": True,
        "identities": identities,
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
