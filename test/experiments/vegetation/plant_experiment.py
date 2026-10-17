"""Check branching attachments and compare deferred groups with full expansion."""

import argparse
import json
import resource
import subprocess
import time
from dataclasses import asdict, replace
from pathlib import Path

import numpy as np
from axis_surface import axis_mesh
from blade_surface import Blade
from curve_frame import arc_frame
from organ_experiment import write_product
from plant_plan import AxisLevel, PlantPlan
from shoot_plan import Shoot, normal_frame

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sapling(seed=17):
    levels = (
        AxisLevel(1, 1.55, 0.019, 0.006, tip_scale=1, bend=0.08, bend_variation=0.035),
        AxisLevel(
            9,
            0.8,
            0.006,
            0.0018,
            tip_scale=0.45,
            first=0.3,
            last=0.98,
            leaf_internode=0.06,
            leaf_first=0.55,
        ),
        AxisLevel(
            6,
            0.43,
            0.0018,
            0.00045,
            tip_scale=0.5,
            first=0.25,
            last=0.94,
            angle=0.95,
            bend=0.42,
            leaf_internode=0.03,
        ),
    )
    blades = tuple(
        Blade(
            lobes=6,
            length=0.12 * (0.85 + 0.15 * variant / 7),
            width=0.06 * (0.88 + 0.12 * ((variant * 3) % 8) / 7),
            fold=0.045 + 0.085 * variant / 7,
            bend=0.18 + 0.27 * variant / 7,
            twist=(variant - 3.5) * 0.08,
        )
        for variant in range(8)
    )
    return PlantPlan(seed, levels, blades)


def assert_same_group(left, right):
    assert left.keys() == right.keys()
    for name in left:
        np.testing.assert_array_equal(left[name], right[name])


def check_plant(plant):
    summary = plant.summary()
    assert not plant._axes and not plant._leaves
    assert plant.expand(summary["axes"] - 1, summary["leaves"]) is None
    assert plant.expand(summary["axes"], summary["leaves"] - 1) is None
    assert plant.leaves((3, 1), 0) is None
    assert not plant._axes and not plant._leaves
    paths = tuple(plant.paths())
    detail = plant.expand(summary["axes"], summary["leaves"])
    assert len(detail["axes"]) == summary["axes"] == 64
    assert summary["leaves"] == 571
    np.testing.assert_array_equal([state.id for state in detail["axes"]], np.arange(64))
    all_ids = np.concatenate([group["ids"] for group in detail["groups"]])
    all_variants = np.concatenate([group["variants"] for group in detail["groups"]])
    np.testing.assert_array_equal(
        all_ids, np.arange(summary["leaves"], dtype=np.uint64)
    )
    np.testing.assert_array_equal(
        np.bincount(all_variants, minlength=8), summary["variant_counts"]
    )
    independently_summed_area = sum(
        plant.blades[int(variant)].area() for variant in all_variants
    )
    np.testing.assert_allclose(
        independently_summed_area, summary["leaf_area_m2"], rtol=2e-14
    )
    max_attachment_error = 0.0
    blade_meshes = [blade.mesh(32, across=2) for blade in plant.blades]
    full_radius = summary["root_bound_radius_m"]
    measured_radius = 0.0
    geometry_by_path = {}
    for state in detail["axes"]:
        np.testing.assert_allclose(state.frame.T @ state.frame, np.eye(3), atol=1e-15)
        np.testing.assert_allclose(np.linalg.det(state.frame), 1, atol=1e-15)
        base_radius, tip_radius = plant.radii(state.path)
        mesh = axis_mesh(state.length, state.bend, base_radius, tip_radius)
        coarse = axis_mesh(
            state.length, state.bend, base_radius, tip_radius, segments=12
        )
        np.testing.assert_array_equal(
            mesh["vertices"][:-2].reshape(25, 8, 3)[::2],
            coarse["vertices"][:-2].reshape(13, 8, 3),
        )
        points = mesh["vertices"] @ state.frame.T + state.origin
        geometry_by_path[state.path] = [points]
        measured_radius = max(
            measured_radius, float(np.linalg.norm(points, axis=1).max())
        )
        if state.path:
            parent = plant.axis(state.path[:-1])
            centre = arc_frame(parent.length, parent.bend, 0, state.parent_parameter)[0]
            np.testing.assert_array_equal(
                state.origin, parent.origin + parent.frame @ centre
            )
        group = plant.leaves(state.path, summary["leaves"])
        if group is None:
            continue
        roots = group["origins"] - plant.blades[0].petiole * group["frames"][:, :, 1]
        error = np.linalg.norm(roots - group["nodes"], axis=1).max()
        max_attachment_error = max(max_attachment_error, float(error))
        assert error < 5e-16
        centres = arc_frame(state.length, state.bend, 0, group["stem_parameter"])[0]
        np.testing.assert_allclose(
            group["nodes"], centres @ state.frame.T + state.origin, atol=1e-15
        )
        np.testing.assert_allclose(np.linalg.det(group["frames"]), 1, atol=1e-15)
        assert np.all(group["frames"][:, 2, 2] >= -1e-15)
        for origin, frame, variant in zip(
            group["origins"], group["frames"], group["variants"]
        ):
            points = blade_meshes[int(variant)]["vertices"] @ frame.T + origin
            geometry_by_path[state.path].append(points)
            measured_radius = max(
                measured_radius, float(np.linalg.norm(points, axis=1).max())
            )
        assert not state.origin.flags.writeable and not state.frame.flags.writeable
        assert all(not value.flags.writeable for value in group.values())
    assert measured_radius <= full_radius
    for path in paths:
        local_summary = plant.summary(path)
        descendants = [
            candidate for candidate in paths if candidate[: len(path)] == path
        ]
        groups = [
            plant.leaves(candidate, summary["leaves"]) for candidate in descendants
        ]
        groups = [group for group in groups if group is not None]
        assert local_summary["axes"] == len(descendants)
        variants = np.concatenate([group["variants"] for group in groups])
        assert local_summary["leaves"] == len(variants)
        np.testing.assert_array_equal(
            np.bincount(variants, minlength=8), local_summary["variant_counts"]
        )
        anchor = plant.axis(path).origin
        points = np.concatenate(
            [
                points
                for candidate in descendants
                for points in geometry_by_path[candidate]
            ]
        )
        assert (
            np.linalg.norm(points - anchor, axis=1).max()
            <= local_summary["root_bound_radius_m"]
        )
    other = PlantPlan(plant.seed, plant.levels, plant.blades)
    assert other.identity == plant.identity
    for path in reversed(paths):
        np.testing.assert_array_equal(other.axis(path).origin, plant.axis(path).origin)
        left, right = (
            other.leaves(path, summary["leaves"]),
            plant.leaves(path, summary["leaves"]),
        )
        if left is not None:
            assert_same_group(left, right)
            assert other.leaves(path, summary["leaves"]) is left
    return summary | {
        "max_attachment_error_m": max_attachment_error,
        "measured_radius_m": measured_radius,
    }


def check_alignment_and_rejections():
    hint = (0.0, 0.0, 1.0)
    directions = np.array(
        [(10.0**power, 0.0, 1.0) for power in range(-12, 0)] + [(0, 0, 1)]
    )
    frames = normal_frame(directions, hint)
    np.testing.assert_allclose(
        np.einsum("nji,njk->nik", frames, frames),
        np.broadcast_to(np.eye(3), frames.shape),
        rtol=0,
        atol=1e-15,
    )
    original = Shoot(normal_hint=hint).detail(100)
    tilted = Shoot(normal_hint=hint, normal_alignment=0.65).detail(100)
    assert np.all(
        np.abs(tilted["frames"][:, 2, 1]) <= np.abs(original["frames"][:, 2, 1]) + 1e-15
    )
    np.testing.assert_array_equal(original["nodes"], tilted["nodes"])
    plant = sapling()
    invalid = [
        lambda: replace(plant.levels[1], branches=0),
        lambda: replace(plant.levels[1], branches=65),
        lambda: replace(plant.levels[1], length=float("nan")),
        lambda: replace(plant.levels[1], leaf_internode=0),
        lambda: replace(plant.levels[1], leaf_sky_alignment=1),
        lambda: replace(plant, seed=-1),
        lambda: replace(plant, seed=2**64),
        lambda: replace(plant, levels=plant.levels * 3),
        lambda: replace(plant, levels=(replace(plant.levels[0], branches=2),)),
        lambda: replace(
            plant, levels=(plant.levels[0], replace(plant.levels[1], base_radius=0.01))
        ),
        lambda: replace(
            plant, blades=(plant.blades[0], replace(plant.blades[1], petiole=0.01))
        ),
        lambda: plant.axis((9,)),
        lambda: plant.axis((0, 6)),
        lambda: plant.expand(-1, 100),
        lambda: plant.leaves((0,), 1.5),
        lambda: Shoot(normal_hint=(0.0, 0.0, 0.0)),
        lambda: Shoot(normal_alignment=0.5),
        lambda: Shoot(normal_hint=hint, normal_alignment=1),
    ]
    for case in invalid:
        try:
            case()
        except ValueError:
            continue
        raise AssertionError("invalid plant or detail request was accepted")
    return {
        "invalid_cases_rejected": len(invalid),
        "sky_alignment_reduces_vertical_leaf_pitch": True,
    }


def check_large_summary():
    base = sapling()
    levels = (base.levels[0],) + tuple(
        replace(
            base.levels[1],
            branches=16,
            base_radius=0.0018,
            tip_radius=0.0018,
            first=0.2,
            last=0.8,
            leaf_internode=0.04,
        )
        for _ in range(6)
    )
    plant = PlantPlan(base.seed, levels, base.blades)
    start = time.perf_counter()
    summary = plant.summary()
    elapsed = (time.perf_counter() - start) * 1000
    assert summary["axes"] == sum(16**depth for depth in range(7))
    assert summary["leaves"] > 100_000_000
    assert plant.expand(1000, 1000) is None
    assert not plant._axes and not plant._leaves
    return summary | {
        "elapsed_ms": elapsed,
        "scope": "count stress case, validated recipe and shared blade areas warm; not a plausible plant or forest rendering",
        "generated_axes": 0,
        "generated_leaf_groups": 0,
    }


def retained_bytes(plant):
    return sum(
        state.origin.nbytes + state.frame.nbytes for state in plant._axes.values()
    ) + sum(
        array.nbytes for group in plant._leaves.values() for array in group.values()
    )


def compare_requests():
    template, path = sapling(), (3, 1)
    summary = template.summary()
    counts = template.slot_leaf_counts[2][path[-1]]
    cases = []
    expected = template.leaves(path, counts)
    for method in ("deferred", "full"):
        timings, repetitions = [], []
        for _ in range(5):
            plant = PlantPlan(template.seed, template.levels, template.blades)
            start = time.perf_counter()
            if method == "full":
                plant.expand(summary["axes"], summary["leaves"])
            actual = plant.leaves(path, counts)
            timings.append((time.perf_counter() - start) * 1000)
            start = time.perf_counter()
            assert plant.leaves(path, counts) is actual
            repetitions.append((time.perf_counter() - start) * 1000)
            assert_same_group(expected, actual)
        cases.append(
            {
                "method": method,
                "first_ms": timings,
                "repeat_ms": repetitions,
                "first_median_ms": float(np.median(timings)),
                "repeat_median_ms": float(np.median(repetitions)),
                "retained_array_bytes": retained_bytes(plant),
                "generated_axes": len(plant._axes),
                "generated_leaf_groups": len(plant._leaves),
                "requested_leaves": counts,
            }
        )
    return {
        "scope": "same requested twig and exact poses; no meshing, rendering or Python object overhead",
        "cases": cases,
    }


def make_scenes(output, plant, commit, modified):
    summary = plant.summary()
    detail = plant.expand(summary["axes"], summary["leaves"])
    products = []
    for state in detail["axes"]:
        base_radius, tip_radius = plant.radii(state.path)
        products.append(
            write_product(
                output,
                f"axis-{state.id}",
                axis_mesh(
                    state.length,
                    state.bend,
                    base_radius,
                    tip_radius,
                    segments=24,
                ),
                state.origin[None, :],
                state.frame[None, :, :],
                "wood",
            )
        )
    groups = detail["groups"]
    combined = {
        key: np.concatenate([group[key] for group in groups]) for key in groups[0]
    }
    np.savez_compressed(output / "canonical-leaves.npz", **combined)
    for variant, blade in enumerate(plant.blades):
        selected = combined["variants"] == variant
        products.append(
            write_product(
                output,
                f"leaf-{variant}",
                blade.mesh(64, across=2),
                combined["origins"][selected],
                combined["frames"][selected],
                "leaf",
            )
        )
    branch = plant.axis((3,))
    target = (
        branch.origin + branch.frame @ arc_frame(branch.length, branch.bend, 0, 0.65)[0]
    )
    scenes = [
        {
            "name": "sapling",
            "title": "Jungbaum mit Hauptästen, Zweigen und Einzelblättern",
            "camera": [2.7, -3.8, 1.85],
            "target": [0, 0, 0.98],
            "fov_y": 30,
        },
        {
            "name": "sapling-branch",
            "title": "Nahansicht derselben kanonischen Ast- und Blattgruppen",
            "camera": (target + np.array((0.7, -0.85, 0.4))).tolist(),
            "target": target.tolist(),
            "fov_y": 42,
        },
    ]
    for scene in scenes:
        scene.update(
            products=products,
            ground_size_m=50,
            resolution=[2560, 1440],
            samples=32,
            source_commit=commit,
            source_modified=modified,
            plan_identity=plant.identity,
        )
        (output / f"{scene['name']}.json").write_text(
            json.dumps(scene, indent=2) + "\n"
        )
    (output / "plan.json").write_text(
        json.dumps(
            {
                "seed": plant.seed,
                "levels": [asdict(level) for level in plant.levels],
                "blades": [asdict(blade) for blade in plant.blades],
                "summary": summary,
                "commit": commit,
            },
            indent=2,
        )
        + "\n"
    )
    return [f"{scene['name']}.json" for scene in scenes]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/plants")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks = [check_plant(sapling(seed)) for seed in (17, 31, 999)]
    validation = check_alignment_and_rejections()
    large = check_large_summary()
    requests = compare_requests()
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    modified = bool(
        subprocess.check_output(
            [
                "git",
                "status",
                "--porcelain",
                "--",
                str(HERE / "plant_plan.py"),
                str(HERE / "plant_experiment.py"),
                str(HERE / "shoot_plan.py"),
                str(HERE / "axis_surface.py"),
                str(HERE / "blade_surface.py"),
                str(HERE / "curve_frame.py"),
                str(HERE / "organ_experiment.py"),
            ],
            cwd=ROOT,
            text=True,
        ).strip()
    )
    scenes = make_scenes(args.output, sapling(), commit, modified)
    report = {
        "checks": checks,
        "validation": validation,
        "large_summary": large,
        "requests": requests,
        "scenes": scenes,
        "commit": commit,
        "source_modified": modified,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "peak_scope": "whole Linux local experiment; no Blender, GPU or native runtime",
    }
    (args.output / "plants.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
