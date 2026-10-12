"""Complete local collars, fair sampler costs and independent plant checks."""

import argparse
import hashlib
import importlib.metadata
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from branch_collar import collar_products
from collar_checks import analytic_checks, check_local_meshes
from collar_shading import blend_collar_normals, tube_reference
from collar_union import local_union
from plant_experiment import sapling
from plant_surface import axis_products, solid_from_mesh, union_mesh
from plant_surface_checks import plant_checks
from plant_surface_experiment import scenes


def sample(plant):
    rows, products = {}, {}
    for method in ("dense", "blocked"):
        times = []
        for _ in range(3):
            began = time.perf_counter()
            meshes, work = collar_products(plant, method=method)
            times.append((time.perf_counter() - began) * 1000)
        rows[method] = {
            "median_ms": float(np.median(times)),
            "samples_ms": times,
            "field_point_evaluations": sum(r["field_point_evaluations"] for r in work),
            "interval_block_evaluations": sum(
                r["interval_block_evaluations"] for r in work
            ),
        }
        products[method] = meshes, work
    for left, right in zip(products["dense"][0], products["blocked"][0], strict=True):
        for key in left:
            np.testing.assert_array_equal(left[key], right[key])
    selected = min(rows, key=lambda method: rows[method]["median_ms"])
    return *products[selected], {
        "methods": rows,
        "selected": selected,
        "exact_sampler_meshes": True,
    }


def compare(seed):
    began = time.perf_counter()
    plant = sapling(seed)
    axes = axis_products(plant, sides=16)
    source_ms = (time.perf_counter() - began) * 1000
    meshes, plans, sampler = sample(plant)
    checked = check_local_meshes(plant, meshes, plans)
    paths = tuple(plant.paths())
    indices = {path: index for index, path in enumerate(paths)}
    began = time.perf_counter()
    references = [tube_reference(mesh, method="embree") for mesh in axes]
    shading = []
    for index, plan in enumerate(plans):
        path = plan["path"]
        state = plant.axis(path)
        parent_level = plant.levels[len(path) - 1]
        parent_radius = parent_level.base_radius + state.parent_parameter * (
            parent_level.tip_radius - parent_level.base_radius
        )
        meshes[index], work = blend_collar_normals(
            meshes[index],
            [references[indices[path[:-1]]], references[indices[path]]],
            parent_radius,
            plant.levels[len(path)].base_radius,
        )
        shading.append(work)
    shading_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    mesh, normal_work = local_union(axes + meshes, with_work=True)
    union_ms = (time.perf_counter() - began) * 1000
    final_checks = plant_checks(plant, axes + meshes, mesh)
    assert final_checks["faces_against_corner_normals"] == 0
    original = solid_from_mesh(union_mesh(axes))
    current = solid_from_mesh(mesh)
    removed, added = (original - current).volume(), (current - original).volume()
    assert removed <= 1e-10 and added > 0
    return (
        plant,
        mesh,
        {
            "seed": seed,
            "plan_identity": plant.identity,
            "source_ms": source_ms,
            "samplers": sampler,
            "local_checks": checked,
            "checks": final_checks,
            "shading_ms": shading_ms,
            "ray_method": "embreex-4.4.0-local-float32-double-refinement",
            "union_ms": union_ms,
            "normal_work": normal_work,
            "shading": shading,
            "collar_plans": plans,
            "removed_volume_m3": removed,
            "added_volume_m3": added,
            "axes": len(axes),
            "leaves": plant.summary()["leaves"],
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/collars")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks = analytic_checks()
    cases = []
    for seed in (17, 23, 41):
        plant, mesh, row = compare(seed)
        cases.append(row)
        if seed == 17:
            displayed = plant, mesh
        print(
            seed,
            row["samplers"]["selected"],
            round(row["shading_ms"]),
            round(row["union_ms"]),
            row["checks"]["triangles"],
            flush=True,
        )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
    )
    scenes(args.output, *displayed, commit, modified)
    report = {
        "commit": commit,
        "source_modified": modified,
        "cases": cases,
        "checks": checks,
        "packages": {
            p: importlib.metadata.version(p)
            for p in ("numpy", "scipy", "scikit-image", "manifold3d", "embreex")
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("*.py")
        },
        "process_peak_rss_with_checks_mib": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        / 1024,
        "scope": "three sampler repetitions per seed on the same fields; shading and union one local run each, Python plus native Manifold. Complete 64-axis/571-leaf calibration plants. Geometric union simplification tolerance 0.1 micrometre, observed old-volume loss budget 1e-10 m3 in these metre-scale fixtures. Normal shaping and observed probes, no globally smooth weld, plant-species or native Outshine/device claim.",
    }
    (args.output / "collars.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
