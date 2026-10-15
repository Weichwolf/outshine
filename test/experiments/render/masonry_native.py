"""Native masonry comparison against independent geometric pixel integration.

The shader remains experimental until selection, metric-frame/raster and cost
checks are complete. Private protocol input uses the existing native probe.
"""

import argparse
import hashlib
import json
import os
import re
import resource
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from facade_footprint import ROOT
from ground_litter_native import capture
from masonry_filter import BRICK_FRACTION, candidates, fixtures
from masonry_reference import controls, exact

METHODS = ("point", "separate_box", "joint_box", "joint4", "joint9", "joint16")
SELECTED_METHOD = 5


def source_colour():
    header = ROOT / "src/generators/building/BuildingMaterials.h"
    colour = np.array(
        [
            float(x.strip().removesuffix("f"))
            for x in re.search(r"Colour\{\{([^}]+)", header.read_text())[1].split(",")
        ]
    )
    pattern = (ROOT / "src/render/shaders/facadePattern.glsl").read_text()
    line = next(
        s for s in pattern.splitlines() if "variant < 6.5" in s and "wallTint" in s
    )
    tint = np.array(
        [float(x) for x in re.search(r"wallTint = vec3\(([^)]+)", line)[1].split(",")]
    )
    return colour * tint


def amplitude(colour):
    positive = np.minimum(colour / 0.16, (1 - colour) / 0.84)
    negative = np.minimum(colour / 0.84, (1 - colour) / 0.16)
    return np.clip(
        (np.array([0.48, 0.46, 0.42]) - colour) / 0.84,
        -np.maximum(negative, 0),
        np.maximum(positive, 0),
    )


def prepare(output, values, baseline=False):
    palette = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette[4:8] = [*source_colour(), 0.9]
    palette.tofile(output / "native.palette")
    records = np.zeros(
        len(values), dtype=[("data", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    records["data"] = values[:, :6]
    records["which"] = values[:, 6]
    records.tofile(output / "probes.bin")
    glslang = os.environ.get("GLSLANG", "glslangValidator")
    for method in range(len(METHODS) + int(baseline)):
        command = [
            "nice",
            "-n",
            "10",
            glslang,
            "-V",
            "-Isrc",
            "-Isrc/render/shaders",
            "-Isrc/content/shade",
            "-Itest/experiments/render",
            f"-DMASONRY_METHOD={method}",
            "test/experiments/render/masonry_probe.comp",
            "-o",
            str(output / f"filter-{method}.spv"),
        ]
        result = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, check=False, timeout=60
        )
        (output / f"compile-{method}.log").write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (command, result.stdout, result.stderr)


def references(output, values, profiles):
    reference_controls = controls()
    results, work, costs = [], [], []
    for index, profile in enumerate(profiles):
        selected = values[values[:, 6] == (0, 1, 8)[index], :6].reshape(-1, 3, 2)
        before = time.perf_counter()
        field, clipped = exact(selected, profile)
        results.append(field)
        work.append(clipped)
        costs.append(
            {
                "profile": (0, 1, 8)[index],
                "seconds": time.perf_counter() - before,
                "clipped_rectangles": int(clipped.sum()),
            }
        )
    np.savez_compressed(
        output / "reference.npz",
        field=np.concatenate(results),
        work=np.concatenate(work),
    )
    (output / "reference-cost.json").write_text(
        json.dumps(
            {
                "scope": "Whole CPU reference child; imports/NumPy/Shapely included, no GPU or native allocation claim",
                "profiles": costs,
                "geometric_reference_controls": reference_controls,
                "peak_process_rss_bytes": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss
                * (1 if sys.platform == "darwin" else 1024),
            },
            indent=2,
        )
        + "\n"
    )


def colour_controls(output, binary):
    output.mkdir(exist_ok=True)
    # Same actual shader, with a complete output on each bounded material cohort.
    values = np.array(
        [
            [0.037, 0.011, 0.0025, 0.0, 0.0, 0.0025, 0],
            [0.037, 0.011, 1.0, 0.0, 0.0, 0.32, 0],
            [0.037, 0.011, 0.01, 0.0, 0.0, 0.01, 7],
            [0.037, 0.011, 0.01, 0.0, 0.0, 0.01, 1],
            [0.037, 0.04, 1.0, 0.0, 0.0, 0.0, 0],
            [0.037, 0.0, 1.0, 0.0, 0.0, 0.0, 0],
        ]
    )
    prepare(output, values)
    records = []
    for name, colour, roughness in (
        ("black", [0, 0, 0], 0.0),
        ("white", [1, 1, 1], 1.0),
        ("mixed", [0, 0.5, 1], 0.9),
        ("near_limits", [0.0001, 0.9999, 0.001], 0.99),
        ("native", source_colour(), 0.9),
    ):
        palette = np.fromfile(output / "native.palette", dtype="<f4")
        palette[4:8] = [*colour, roughness]
        palette.tofile(output / f"{name}.palette")
        actual = capture(binary, output, SELECTED_METHOD, name, len(values), 13)
        assert (
            np.isfinite(actual).all()
            and actual[:, :4].min() >= -2e-6
            and actual[:, :4].max() <= 1 + 2e-6
        )
        assert abs(actual[1, 7]) < 2e-6 and actual[2, 7] == 0
        np.testing.assert_allclose(actual[4:6, 7], [-0.12, 0.84], rtol=0, atol=2e-6)
        np.testing.assert_allclose(
            actual[1, :4], [*colour, roughness], rtol=0, atol=2e-6
        )
        records.append(
            {
                "colour": name,
                "range": [float(actual[:, :4].min()), float(actual[:, :4].max())],
                "far_mean_error": float(
                    np.max(np.abs(actual[1, :4] - [*colour, roughness]))
                ),
            }
        )
    return records


def orientation_controls(output, binary, values):
    output.mkdir(exist_ok=True)
    original = values[np.linspace(0, len(values) - 1, 64).astype(int)]
    flipped = original.copy()
    flipped[:, 2:4] *= -1
    swapped = original.copy()
    swapped[:, 2:4], swapped[:, 4:6] = original[:, 4:6], original[:, 2:4]
    prepare(output, np.concatenate([original, flipped, swapped]))
    actual = capture(binary, output, SELECTED_METHOD, "native", len(original) * 3, 13)
    initial, reverse, exchange = np.split(actual, 3)
    error = float(
        max(np.max(np.abs(initial - reverse)), np.max(np.abs(initial - exchange)))
    )
    assert error < 2e-4, error
    return {"cases": len(actual), "max_sign_swap_error": error}


def precision_control(output, binary, values, profiles):
    shader = output / "filter-7.spv"
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
        "-DMASONRY_METHOD=3",
        "-DMASONRY_STABLE_PHASE=0",
        "test/experiments/render/masonry_probe.comp",
        "-o",
        str(shader),
    ]
    p = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=60, check=False
    )
    (output / "precision-compile.log").write_text(p.stdout + p.stderr)
    assert p.returncode == 0, p.stderr
    actual = capture(binary, output, 7, "native", len(values), 13)
    expected = []
    for i, style in enumerate((0, 1, 8)):
        at, dx, dy = (
            values[values[:, 6] == style, :6].reshape(-1, 3, 2).transpose(1, 0, 2)
        )
        expected.extend(candidates(at, dx, dy, profiles[i])["joint4"])
    error = float(np.max(np.abs(actual[:, 7] - expected)))
    assert error > 2e-4, ("negative precision control unexpectedly passed", error)
    return {
        "stable_phase": False,
        "method": "joint4",
        "max_parity_error": error,
        "rejected_by_unchanged_tolerance": True,
    }


def plot(output, report):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(1, 3, figsize=(13, 4), constrained_layout=True)
    for axis, profile in zip(axes, report["profiles"], strict=True):
        rows = profile["methods"]
        axis.bar(np.arange(len(rows)), [row["max_rgb_error"] for row in rows])
        axis.set_xticks(
            np.arange(len(rows)), [row["method"] for row in rows], rotation=45
        )
        axis.set_title(f"Native profile {profile['style']}")
        axis.set_ylabel("Masonry-only linear RGB error / exact pixel")
        axis.grid(axis="y", alpha=0.2)
    figure.savefig(output / "filter-comparison.pdf")
    plt.close(figure)


def compare(values, profiles, tracks, reference, actual):
    reports, parity_max = [], 0.0
    for index, style in enumerate((0, 1, 8)):
        selected = values[:, 6] == style
        rows = []
        at, dx, dy = values[selected, :6].reshape(-1, 3, 2).transpose(1, 0, 2)
        expected = candidates(at, dx, dy, profiles[index])
        for method in METHODS:
            measured = actual[method][selected]
            parity = float(np.max(np.abs(measured[:, 7] - expected[method])))
            assert parity < 2e-4, (style, method, parity)
            parity_max = max(parity, parity_max)
            residual = measured[:, 7] - reference[selected]
            motion = []
            for track in tracks:
                start = track["begin"]
                end = start + track["steps"] * track["points"]
                errors = residual[start:end].reshape(track["steps"], track["points"])
                motion.append(
                    {
                        "kind": track["kind"],
                        "false_step_rgb_rms": float(
                            np.sqrt(np.mean(np.diff(errors, axis=0) ** 2))
                            * np.max(np.abs(amplitude(source_colour())))
                        ),
                    }
                )
            rows.append(
                {
                    "method": method,
                    "max_field_error": float(np.max(np.abs(residual))),
                    "max_rgb_error": float(
                        np.max(np.abs(residual))
                        * np.max(np.abs(amplitude(source_colour())))
                    ),
                    "rms_field_error": float(np.sqrt(np.mean(residual**2))),
                    "motion": motion,
                }
            )
        reports.append({"style": style, "methods": rows})
    return reports, parity_max


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
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    fixture, tracks, profiles = fixtures()
    # Match actual GPU float inputs before the independent geometry reference.
    values = (
        np.concatenate(
            [
                np.column_stack([fixture.reshape(-1, 6), np.full(len(fixture), style)])
                for style in (0, 1, 8)
            ]
        )
        .astype(np.float32)
        .astype(float)
    )
    profiles = profiles.astype(np.float32).astype(float)
    if args.reference_only:
        references(output, values, profiles)
        return
    if args.binary is None:
        parser.error("actual native execution requires --binary")
    prepare(output, values)
    np.savez_compressed(output / "cases.npz", values=values, profiles=profiles)
    (output / "reference.npz").unlink(missing_ok=True)
    child = [
        "nice",
        "-n",
        "10",
        sys.executable,
        str(Path(__file__)),
        "--output",
        str(output),
        "--reference-only",
    ]
    subprocess.run(child, cwd=ROOT, check=True)
    reference = np.load(output / "reference.npz")["field"]
    actual = {
        method: capture(args.binary.resolve(), output, index, "native", len(values), 13)
        for index, method in enumerate(METHODS)
    }
    for pixels in actual.values():
        assert (
            np.isfinite(pixels).all()
            and pixels[:, :4].min() >= -2e-6
            and pixels[:, :4].max() <= 1 + 2e-6
        )
    reports, parity_max = compare(values, profiles, tracks, reference, actual)
    orientations = orientation_controls(
        output / "orientation", args.binary.resolve(), values
    )
    precision = precision_control(output, args.binary.resolve(), values, profiles)
    controls = colour_controls(output / "controls", args.binary.resolve())
    sources = [
        Path(__file__),
        *[
            Path(__file__).with_name(name)
            for name in (
                "masonry_filter.py",
                "masonry_reference.py",
                "masonry_filter.glsl",
                "masonry_probe.comp",
            )
        ],
        ROOT / "src/generators/building/BuildingMaterials.h",
        ROOT / "src/content/shade/FacadeProfiles.inc",
        ROOT / "test/outshine/data/materials/ground-litter.palette",
        ROOT / "test/harness/render/NativeGroundLitterProbe.cpp",
        ROOT / "src/render/shaders/facadePattern.glsl",
    ]
    report = {
        "scope": __doc__,
        "native_executed": True,
        "cases": len(values),
        "methods": len(METHODS),
        "selected_method": METHODS[SELECTED_METHOD],
        "max_parity_error": parity_max,
        "precision_negative_control": precision,
        "orientation_controls": orientations,
        "source_colour": source_colour().tolist(),
        "mortar_amplitude": amplitude(source_colour()).tolist(),
        "brick_mean_fraction": float(BRICK_FRACTION),
        "profiles": reports,
        "colour_controls": controls,
        "cpu_reference_cost": json.loads((output / "reference-cost.json").read_text()),
        "work_per_candidate": {
            "point": "one point field plus existing window filter",
            "separate_box": "four periodic integrals plus existing window filter",
            "joint_box/4/9/16": "4 brick and6 facade periodic integrals as written per subcell (only opening/band contribute; unused pane may be eliminated), plus the same final window coverage; no hashes/textures",
        },
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
    }
    plot(output, report)
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cases": len(values),
                "max_parity_error": parity_max,
                "native_executed": True,
            }
        )
    )


if __name__ == "__main__":
    main()
