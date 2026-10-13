"""Verify rounded complete graph wood and compare equivalent expensive procedures."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from closest_checks import check_closest
from graph_collars import graph_collars
from graph_wood_axes import wood_axes
from graph_wood_checks import check_wood, verify_seed
from graph_wood_material import wood_material
from graph_wood_scenes import make_scenes
from graph_wood_surface import joined_wood
from join_normal_checks import check_weighting
from spatial_examples import spatial_graph
from spine_closest import closest_spine


def compare_projection(axes, mesh):
    root = axes[0]
    points = (mesh["vertices"] - root.pose.origin) @ root.pose.frame / root.pose.scale
    samples = {m: [] for m in ("scalar", "batch", "bounded")}
    outputs = {}
    for _ in range(3):
        for method, times in samples.items():
            began = time.perf_counter()
            outputs[method] = closest_spine(
                root.shape.spine, points, method, with_work=True
            )
            times.append(time.perf_counter() - began)
    for method in ("scalar", "bounded"):
        np.testing.assert_allclose(
            outputs[method][0], outputs["batch"][0], atol=1e-11, rtol=0
        )
        np.testing.assert_allclose(
            outputs[method][1], outputs["batch"][1], atol=1e-11, rtol=0
        )
    return {
        "samples_s": samples,
        "work": {k: v[2] for k, v in outputs.items()},
        "points": len(points),
    }


def compare_material(axes, mesh):
    samples = {m: [] for m in ("dense", "indexed")}
    outputs = {}
    for _ in range(3):
        for method, times in samples.items():
            began = time.perf_counter()
            outputs[method] = wood_material(axes, mesh["vertices"], method)
            times.append(time.perf_counter() - began)
    for name in outputs["dense"][0]:
        np.testing.assert_array_equal(
            outputs["dense"][0][name], outputs["indexed"][0][name]
        )
    return {
        "samples_s": samples,
        "work": {k: v[1] for k, v in outputs.items()},
        "equal_attributes": True,
    }


def compare_sampler(axes):
    samples = {m: [] for m in ("dense", "blocked")}
    outputs = {}
    for _ in range(3):
        for method, times in samples.items():
            began = time.perf_counter()
            outputs[method] = graph_collars(axes, method=method)
            times.append(time.perf_counter() - began)
    for a, b in zip(outputs["dense"][0], outputs["blocked"][0]):
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
    return {
        "samples_s": samples,
        "equal_meshes": True,
        "work": {
            k: sum(row["field_point_evaluations"] for row in v[1])
            for k, v in outputs.items()
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/graph-wood")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    projection_checks = check_closest()
    weighting_checks = check_weighting()
    reports = []
    selected = None
    for seed in (17, 23, 47):
        result, checks = verify_seed(seed)
        reports.append({"seed": seed, "checks": checks, "work": result[1]})
        if seed == 17:
            selected = result
    coarse = joined_wood(spatial_graph(), "coarse")
    coarse_checks = check_wood(coarse[0], coarse[2], coarse[3], "coarse")
    assert coarse[1]["collars"] == 0
    assert wood_axes(spatial_graph(), max_axes=1) is None
    assert wood_axes(spatial_graph(), max_triangles=0) is None
    projection = compare_projection(selected[2], selected[0])
    material = compare_material(selected[2], selected[0])
    sampler = compare_sampler(selected[2])
    scene = make_scenes(args.output, selected[0], coarse[0])
    names = (
        "graph_wood_experiment.py",
        "graph_wood_axes.py",
        "graph_collars.py",
        "graph_wood_material.py",
        "graph_wood_surface.py",
        "graph_wood_checks.py",
        "graph_wood_scenes.py",
        "spine_closest.py",
        "closest_checks.py",
        "collar_union.py",
        "collar_shading.py",
        "surface_rays.py",
        "spatial_axis.py",
        "generator_graph.py",
        "generator_specs.py",
        "join_normals.py",
        "join_normal_checks.py",
    )
    report = {
        "projection_checks": projection_checks,
        "weighting_checks": weighting_checks,
        "seeds": reports,
        "coarse": coarse[1] | {"checks": coarse_checks},
        "projection": projection,
        "material": material,
        "sampler": sampler,
        "scene": scene,
        "process_peak_with_checks_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "original_weld_equivalence_proven": False,
    }
    (args.output / "wood-functions.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "scene": scene,
                "coarse_triangles": coarse[1]["triangles"],
                "projection": projection,
                "material": material,
                "sampler": sampler,
            }
        )
    )


if __name__ == "__main__":
    main()
