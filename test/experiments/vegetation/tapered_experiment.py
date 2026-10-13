"""Generate and compare complete tapered fork surfaces from stable radius plans."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from functools import partial
from pathlib import Path

import numpy as np
from adaptive_checks import kernel_checks
from fork_adaptive import adaptive_mesh
from fork_checks import mesh_checks, ray_mesh_radii
from fork_experiment import fixtures as constant_fixtures
from fork_experiment import probe_directions
from fork_refinement import refined_voxel_mesh
from fork_surface import blocked_voxel_mesh, voxel_mesh
from organ_experiment import UPRIGHT, write_product
from tapered_checks import (
    analytic_taper_checks,
    envelope_checks,
    field_checks,
    rejection_checks,
)
from tapered_fork import TaperedFork


def fixtures():
    tips = ((0.27, 0.045, 0.045), (0.29, 0.055, 0.03), (0.27, 0.04, 0.035))
    return {
        name: TaperedFork(field.endpoints, field.radii, field.blend, tip_radii=radii)
        for (name, field), radii in zip(constant_fixtures().items(), tips, strict=True)
    }


def compare(field):
    directions = probe_directions(field, field.tip_radii)
    exact, _work = field.radial_radius(directions)
    rows, selected, selected_row, reference = [], None, None, None
    methods = (
        partial(voxel_mesh, cells=128),
        partial(blocked_voxel_mesh, cells=128, block_size=8),
        partial(blocked_voxel_mesh, cells=192, block_size=8),
        partial(refined_voxel_mesh, cells=192, rounds=0),
        partial(refined_voxel_mesh, cells=128, rounds=1),
        partial(adaptive_mesh, tolerance=0.004, max_depth=9),
        partial(adaptive_mesh, tolerance=0.002, max_depth=9),
    )
    for build in methods:
        began = time.perf_counter()
        fresh = TaperedFork(
            field.endpoints, field.radii, field.blend, field.origin, field.tip_radii
        )
        mesh, work = build(fresh)
        first_ms = (time.perf_counter() - began) * 1000
        timings = []
        for _ in range(3):
            began = time.perf_counter()
            repeated, _work = build(field)
            timings.append((time.perf_counter() - began) * 1000)
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], repeated[key])
        if work["method"] == "voxel":
            reference = mesh
        elif work["method"] == "blocked" and work["longest_axis_cells"] == 128:
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], reference[key])
            work["exact_dense_mesh"] = True
        checks = mesh_checks(mesh, field)
        error = float(
            np.max(abs(ray_mesh_radii(mesh, directions, field.origin) - exact))
        )
        passed = (
            error <= 0.005
            and work.get("patch_probe_tolerance_met", True)
            and checks["faces_disagreeing_with_analytic_normals"] == 0
        )
        row = (
            work
            | checks
            | {
                "median_ms": float(np.median(timings)),
                "samples_ms": timings,
                "first_product_ms": first_ms,
                "maximum_radial_probe_error_m": error,
                "independent_probe_count": len(directions),
                "same_quality_pass": passed,
            }
        )
        rows.append(row)
        if passed and (
            selected is None or row["median_ms"] < selected_row["median_ms"]
        ):
            selected, selected_row = mesh, row
    if selected is None:
        raise ValueError(
            "no tapered fork method meets the 5 mm sampled shape requirement"
        )
    return selected, {"cases": rows, "selected": selected_row}


def scene(fields, meshes, output, commit, modified):
    constant = constant_fixtures()["symmetric"]
    baseline, _work = blocked_voxel_mesh(constant, 128, 8)
    products = [
        write_product(
            output,
            "constant",
            baseline,
            np.array([[-2.55, 0, 0.94]]),
            UPRIGHT[None],
            "wood",
        )
    ]
    for index, (name, field) in enumerate(fields.items()):
        products.append(
            write_product(
                output,
                name,
                meshes[name],
                np.array([[-0.85 + index * 1.7, 0, 0.75 + field.tip_radii[0]]]),
                UPRIGHT[None],
                "wood",
            )
        )
    recipe = {
        "name": "tapered-forks",
        "title": "Konstante und verjüngte gemeinsame Astgabeln",
        "products": products,
        "camera": [4.8, -12, 4.0],
        "target": [0, 0, 1.0],
        "fov_y": 31,
        "ground_size_m": 50,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "scope": "same straight axes with stable positive endpoint radius plans; analytic envelopes before arbitrary curved parent surfaces, bark, wind or botanical recipes",
    }
    (output / "tapered-forks.json").write_text(json.dumps(recipe, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/tapered")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fields, meshes, results = fixtures(), {}, {}
    checks = (
        envelope_checks()
        | analytic_taper_checks()
        | rejection_checks()
        | kernel_checks(fields)
    )
    for name, field in fields.items():
        meshes[name], result = compare(field)
        results[name] = result | {
            "checks": field_checks(field),
            "base_radii_m": field.radii,
            "tip_radii_m": field.tip_radii,
        }
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    scene(fields, meshes, args.output, commit, modified)
    result = {
        "commit": commit,
        "source_modified": modified,
        "checks": checks,
        "cases": results,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "tapered_fork.py",
                "adaptive_checks.py",
                "fork_adaptive.py",
                "tapered_checks.py",
                "tapered_experiment.py",
                "fork_field.py",
                "fork_surface.py",
                "fork_refinement.py",
                "fork_volume.py",
                "fork_intervals.py",
                "fork_experiment.py",
                "fork_checks.py",
                "spine_frames.py",
                "cube_surface.py",
            )
        },
        "scope": "shared-machine CPU, three repeats plus fresh plan; bounded geometry generation before botanical recipes. Sampled radial errors, not a global surface bound or native device measurement. No original SpeedTree scene equivalence claimed",
    }
    (args.output / "tapered.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({name: item["selected"] for name, item in results.items()}, indent=2)
    )


if __name__ == "__main__":
    main()
