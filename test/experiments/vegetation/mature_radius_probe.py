"""One existing mature recipe: independent full radii/area and bounded near ranges."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from experiment_output import private_output
from mature_conifer_recipes import mature_plan
from mature_density import (
    MAX_AXES,
    MAX_WINDOWS,
    WINDOW_SETTINGS,
    match_area,
    needle_areas,
)
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from outshine_ref.vegetation.curve_frame import arc_frame


def all_radii(plant):
    wood, expected, largest = plant.wood, {}, 0.0
    radii = []
    for path in wood.paths():
        axis, level = wood.axis(path), wood.levels[len(path)]
        if path:
            base, tip = expected[path[:-1]]
            parent = (1 - axis.parent_parameter) * base + axis.parent_parameter * tip
            base = min(level.base_radius, level.radius_parent_ratio * parent)
            target = (base, base / level.base_radius * level.tip_radius)
        else:
            target = (level.base_radius, level.tip_radius)
        actual = wood.radii(path)
        np.testing.assert_allclose(actual, target, rtol=2e-15, atol=2e-17)
        if (
            not 0
            < actual[1]
            <= actual[0]
            <= wood.radius_limits[len(path)][path[-1] if path else 0] + 2e-17
        ):
            raise ValueError("actual radius exceeded the original physical bounds")
        expected[path] = target
        largest = max(largest, max(abs(np.asarray(actual) - target)))
        radii.append(actual)
    return np.asarray(radii), largest


def check_near(plant, windows):
    result = []
    for fraction in (0, 0.5, 1):
        path = tuple(
            round((level.branches - 1) * fraction) for level in plant.wood.levels[1:]
        )
        axis = plant.wood.axis(path)
        detail = plant.foliage(path, 100000, retain=False)
        selected = windows["axis_ids"] == axis.id
        first, counts = (
            windows["first_needle_ids"][selected],
            windows["needle_counts"][selected],
        )
        if not len(first) or int(counts.sum()) != len(detail["ids"]):
            raise ValueError(
                "actual near request does not own its complete window range"
            )
        np.testing.assert_array_equal(
            detail["ids"],
            np.arange(int(first[0]), int(first[-1] + counts[-1]), dtype="u8"),
        )
        t = detail["parent_parameters"]
        centre = arc_frame(axis.length, axis.bend, 0, t)[0] @ axis.frame.T + axis.origin
        base, tip = plant.wood.radii(path)
        measured = np.linalg.norm(detail["short_origins"] - centre, axis=1)
        np.testing.assert_allclose(
            measured, base * (1 - t) + tip * t, atol=2e-12, rtol=0
        )
        result.append(
            {
                "path": path,
                "needles": len(detail["ids"]),
                "axis_id": axis.id,
                "maximum_radius_error_m": float(
                    abs(measured - (base * (1 - t) + tip * t)).max()
                ),
            }
        )
    if plant._groups:
        raise ValueError("bounded near requests retained whole foliage")
    return result


def run(args):
    output = private_output(args.output)
    if output.exists():
        raise ValueError("fresh private single-recipe output required")
    started = time.perf_counter()
    profile = json.loads(args.profile.read_text())
    areas, inputs = needle_areas(args.species, args.source)
    target = (
        profile["lai"]
        * np.pi
        * (profile["spread_m"] * (0.85 + args.variant * 0.1) / 2) ** 2
    )
    with patch.object(PlantPlan, "axis", side_effect=AssertionError("preflight axis")):
        plant = mature_plan(args.species, profile, args.variant, args.source)
        plant, density = match_area(plant, areas, target)
        summary = plant.window_summary(**WINDOW_SETTINGS)
        if summary["axes"] > MAX_AXES or summary["windows"] > MAX_WINDOWS:
            raise ValueError("whole original mature budgets exceeded")
        if (
            plant.windows(summary["axes"] - 1, MAX_WINDOWS, **WINDOW_SETTINGS)
            is not None
        ):
            raise ValueError("axis budget must reject before axis work")
    preflight = time.perf_counter() - started
    if plant.wood._axes or plant._groups:
        raise ValueError("preflight created detailed geometry")
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("whole foliage")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("whole needles")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("new needle meshes")),
    ):
        radii, error = all_radii(plant)
        windows = plant.windows(MAX_AXES, MAX_WINDOWS, **WINDOW_SETTINGS)
    if len(radii) != summary["axes"] or len(windows["origins"]) != summary["windows"]:
        raise ValueError("complete actual radius/window counts changed")
    expected_counts = np.zeros(len(areas), dtype="u8")
    needles = sum(plant.needle_counts)
    for first in range(0, needles, 65536):
        ids = np.arange(first, min(first + 65536, needles), dtype="u8")
        expected_counts += np.bincount(
            ((ids + plant.wood.seed) % len(areas)).astype(int), minlength=len(areas)
        ).astype("u8")
    np.testing.assert_array_equal(
        expected_counts, plant.area_summary(areas)["variant_counts"]
    )
    near = check_near(plant, windows)
    output.mkdir(parents=True)
    np.savez_compressed(output / "windows.npz", **windows)
    np.savez_compressed(output / "radii.npz", radii=radii)
    (output / "recipe.json").write_text(json.dumps(plant.recipe(), indent=2) + "\n")
    paths = [args.profile, args.source / args.species / "whole-plan.json"]
    inputs |= {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "mature_radius_probe.py",
                "mature_conifer_recipes.py",
                "mature_density.py",
            )
        },
        "identity": plant.identity,
        "input_sha256": inputs,
        "summary": summary,
        "density": density,
        "near": near,
        "maximum_independent_radius_error_m": error,
        "independent_variant_counts": expected_counts.tolist(),
        "preflight_seconds": preflight,
        "complete_probe_seconds": time.perf_counter() - started,
        "window_array_bytes": sum(a.nbytes for a in windows.values()),
        "radius_array_bytes": radii.nbytes,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "One unchanged mature recipe and bounded full radii/window/area checks; no eight-form bake, full organs, new morphology, statistical fidelity, stand or native admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        {
            "axes": len(radii),
            "windows": len(windows["origins"]),
            "needles": needles,
            "seconds": report["complete_probe_seconds"],
        },
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--species", choices=("picea-abies", "pinus-sylvestris"), required=True
    )
    parser.add_argument("--variant", type=int, choices=range(4), required=True)
    run(parser.parse_args())
