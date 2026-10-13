"""Compare complete 64-axis wood unions while preserving all canonical organs."""

import argparse
import hashlib
import importlib.metadata
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from organ_experiment import write_product
from plant_experiment import make_scenes, sapling
from plant_surface import axis_products, solid_from_mesh, union_mesh
from plant_surface_checks import analytic_checks, plant_checks


def compare(seed):
    began = time.perf_counter()
    plant = sapling(seed)
    products = axis_products(plant, sides=16)
    source_ms = (time.perf_counter() - began) * 1000
    rows, meshes = {}, {}
    for method in ("batch", "sequential"):
        times = []
        for _ in range(3):
            began = time.perf_counter()
            mesh = union_mesh(products, method)
            times.append((time.perf_counter() - began) * 1000)
        checks = plant_checks(plant, products, mesh)
        assert checks["faces_against_corner_normals"] == 0
        rows[method] = checks | {
            "median_ms": float(np.median(times)),
            "samples_ms": times,
        }
        meshes[method] = mesh
    a, b = (solid_from_mesh(meshes[method]) for method in rows)
    difference = (a - b).volume() + (b - a).volume()
    assert difference < 1e-13
    selected = min(rows, key=lambda name: rows[name]["median_ms"])
    return (
        plant,
        meshes[selected],
        {
            "seed": seed,
            "plan_identity": plant.identity,
            "source_ms": source_ms,
            "source_triangles": sum(len(m["faces"]) for m in products),
            "source_product_bytes": sum(a.nbytes for m in products for a in m.values()),
            "methods": rows,
            "selected": selected,
            "kernel_symmetric_difference_m3": difference,
            "axes": len(products),
            "leaves": plant.summary()["leaves"],
        },
    )


def scenes(output, plant, mesh, commit, modified):
    make_scenes(output, plant, commit, modified)
    original = json.loads((output / "sapling.json").read_text())
    wood = write_product(
        output, "united-wood", mesh, np.zeros((1, 3)), np.eye(3)[None], "wood"
    )
    original["products"] = [wood] + [
        p for p in original["products"] if p["surface"] != "wood"
    ]
    original["name"] = "united-sapling"
    original["title"] = (
        "64 Holzachsen als gemeinsamer Körper · 571 unveränderte Blätter"
    )
    (output / "united-sapling.json").write_text(json.dumps(original, indent=2) + "\n")
    state = plant.axis((4,))
    target = state.origin
    view = state.frame[:, 0] + 0.35 * state.frame[:, 1] + np.array((0, 0, 0.3))
    view /= np.linalg.norm(view)
    close = original | {
        "name": "united-attachment",
        "title": "Nahansicht der vereinigten Eltern-/Astoberfläche",
        "products": [wood],
        "camera": (target + 0.16 * view).tolist(),
        "target": target.tolist(),
        "fov_y": 38,
    }
    (output / "united-attachment.json").write_text(json.dumps(close, indent=2) + "\n")
    products = []
    for index, source in enumerate(axis_products(plant, sides=16)):
        products.append(
            write_product(
                output,
                f"source-{index}",
                source,
                np.zeros((1, 3)),
                np.eye(3)[None],
                "wood",
            )
        )
    before = close | {"name": "overlapping-attachment", "products": products}
    (output / "overlapping-attachment.json").write_text(
        json.dumps(before, indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/plant-surfaces"),
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = []
    for seed in (17, 23, 41):
        plant, mesh, row = compare(seed)
        cases.append(row)
        if seed == 17:
            displayed = plant, mesh
        print(
            seed,
            row["selected"],
            row["methods"][row["selected"]]["median_ms"],
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
        "checks": analytic_checks(),
        "simplification_tolerance_m": 1e-8,
        "packages": {n: importlib.metadata.version(n) for n in ("numpy", "manifold3d")},
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__).with_name(name)
                for name in (
                    "plant_surface.py",
                    "plant_surface_checks.py",
                    "plant_surface_experiment.py",
                    "plant_plan.py",
                    "axis_surface.py",
                    "curve_frame.py",
                    "tube_topology.py",
                    "plant_experiment.py",
                    "blade_surface.py",
                    "shoot_plan.py",
                )
            ]
        },
        "scope": "Python orchestration with native Manifold kernel; three local repetitions per method/seed, canonical axis construction reported separately. Occupancy checks independent solid angles; symmetric difference uses the same Boolean library. Piecewise planar union, no organic fillet, native Outshine or device performance claim.",
    }
    (args.output / "plant-surfaces.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
