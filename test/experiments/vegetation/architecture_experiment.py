"""Compare branching recipes with bounded variation and stable partial requests."""

import argparse
import json
import resource
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from curve_frame import arc_frame
from leaf_geometry_lod import merge_instances
from leaf_sheet import make_sheet
from organ_experiment import write_product
from plant_experiment import assert_same_group
from plant_plan import PlantPlan
from sheet_experiment import assemble_wood, larger_tree

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def broadleaf(seed=17):
    source = larger_tree(seed)
    levels = list(source.levels)
    levels[1] = replace(
        levels[1],
        branches=10,
        length=3.3,
        base_radius=0.060,
        tip_radius=0.020,
        first=0.32,
        last=0.96,
        tip_scale=0.30,
        length_shape="rounded",
        angle=1.02,
        angle_variation=0.30,
        bend=0.48,
        bend_variation=0.22,
    )
    levels[2] = replace(
        levels[2],
        branches=9,
        length=1.35,
        base_radius=0.012,
        tip_radius=0.003,
        first=0.30,
        last=0.94,
        tip_scale=0.45,
        angle=0.92,
        angle_variation=0.30,
        bend=0.45,
        bend_variation=0.22,
        upward_bias=0.22,
    )
    levels[3] = replace(
        levels[3],
        base_radius=0.003,
        tip_radius=0.0008,
        first=0.35,
        last=0.94,
        angle=0.95,
        upward_bias=0.18,
    )
    levels[4] = replace(
        levels[4],
        base_radius=0.0007,
        tip_radius=0.0002,
        first=0.2,
        leaf_internode=0.018,
        leaf_sky_alignment=0.25,
        upward_bias=0.10,
    )
    levels[1:] = [
        replace(level, attachment_jitter=0.42, azimuth_variation=0.65)
        for level in levels[1:]
    ]
    return PlantPlan(seed, tuple(levels), source.blades)


def check_recipe(seed):
    plant = broadleaf(seed)
    summary = plant.summary()
    assert not plant._axes and not plant._leaves
    paths = tuple(plant.paths())
    selected = paths[-1]
    group = plant.leaves(selected, summary["leaves"])
    assert len(plant._axes) == len(plant.levels) and len(plant._leaves) == 1
    fresh = broadleaf(seed)
    for path in reversed(paths[::67]):
        fresh.leaves(path, summary["leaves"])
    assert_same_group(group, fresh.leaves(selected, summary["leaves"]))
    for level in plant.levels:
        lengths = np.array([level.length_at(slot) for slot in range(level.branches)])
        assert np.all(lengths >= level.length * level.tip_scale - 1e-14)
        assert np.all(lengths <= level.length + 1e-14)
    detail = plant.expand(summary["axes"], summary["leaves"])
    ids = np.concatenate([group["ids"] for group in detail["groups"]])
    np.testing.assert_array_equal(ids, np.arange(summary["leaves"]))
    maximum_error = 0.0
    for axis in detail["axes"]:
        np.testing.assert_allclose(axis.frame.T @ axis.frame, np.eye(3), atol=2e-15)
        assert np.linalg.det(axis.frame) > 0
        children = [plant.axis(path) for path in plant.children(axis.path)]
        if children:
            parameters = np.array([child.parent_parameter for child in children])
            assert np.all(np.diff(parameters) > 0)
        if axis.path:
            parent = plant.axis(axis.path[:-1])
            centre = arc_frame(parent.length, parent.bend, 0, axis.parent_parameter)[0]
            error = np.linalg.norm(axis.origin - parent.origin - parent.frame @ centre)
            maximum_error = max(maximum_error, error)
            assert error < 1e-14
    for group in detail["groups"]:
        radii = np.linalg.norm(group["origins"], axis=1)
        assert radii.max() < summary["root_bound_radius_m"]
    for field, value in (
        ("length_shape", "unknown"),
        ("attachment_jitter", 0.5),
        ("upward_bias", 0.5),
        ("upward_bias", -1),
        ("azimuth_variation", 4),
    ):
        try:
            replace(plant.levels[1], **{field: value})
        except ValueError:
            continue
        raise AssertionError((field, value))
    return {
        "seed": seed,
        "axes": summary["axes"],
        "leaves": summary["leaves"],
        "maximum_attachment_error_m": float(maximum_error),
    }


def scene(output, name, plant, commit, modified):
    summary = plant.summary()
    began = time.perf_counter()
    detail = plant.expand(summary["axes"], summary["leaves"])
    placement_ms = (time.perf_counter() - began) * 1000
    groups = detail["groups"]
    poses = {key: np.concatenate([group[key] for group in groups]) for key in groups[0]}
    wood, wood_stats = assemble_wood(plant, detail)
    products = [
        write_product(
            output, f"{name}-wood", wood, np.zeros((1, 3)), np.eye(3)[None], "wood"
        )
    ]
    began = time.perf_counter()
    for variant, blade in enumerate(plant.blades):
        sheet = make_sheet(blade, variant)
        maps = sheet.save(output, f"{name}-leaf-{variant}")
        mesh = blade.mesh(16, across=1)
        mesh["uv"] = sheet.coordinates(mesh["vertices"])
        selected = poses["variants"] == variant
        merged = merge_instances(
            mesh,
            poses["origins"][selected],
            poses["frames"][selected],
            poses["ids"][selected],
        )
        product = write_product(
            output,
            f"{name}-leaf-{variant}",
            merged,
            np.zeros((1, 3)),
            np.eye(3)[None],
            "leaf",
        )
        product.update(textures=maps, card=False)
        products.append(product)
    meshing_ms = (time.perf_counter() - began) * 1000
    recipe = {
        "name": name,
        "title": "Laubbaum · Kronenarchitektur",
        "products": products,
        "camera": [11, -22, 10],
        "target": [0, 0, 4.7],
        "fov_y": 35,
        "ground_size_m": 300,
        "clip_end_m": 400,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "plan_identity": plant.identity,
    }
    (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
    if name == "architecture-after":
        side = recipe | {"name": "architecture-side", "camera": [-22, -11, 10]}
        (output / "architecture-side.json").write_text(
            json.dumps(side, indent=2) + "\n"
        )
    return {
        "summary": summary,
        "placement_ms": placement_ms,
        "leaf_mesh_chart_write_ms": meshing_ms,
        "wood": wood_stats,
        "triangles": sum(
            p["triangles_per_instance"] * p["instances"] for p in products
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/architecture")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks = [check_recipe(seed) for seed in (2, 17, 81)]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(HERE)], text=True
        ).strip()
    )
    cases = {
        name: scene(args.output, name, factory(), commit, modified)
        for name, factory in (
            ("architecture-before", larger_tree),
            ("architecture-after", broadleaf),
        )
    }
    report = {
        "commit": commit,
        "source_modified": modified,
        "checks": checks,
        "cases": cases,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "Linux Python process, including verification and serialized Blender meshes; not runtime or GPU",
    }
    (args.output / "architecture.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
