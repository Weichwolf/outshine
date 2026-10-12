"""Compare complete shared surfaces from independently curved branch spines."""

import numpy as np
from curved_fork import CurveBranch, CurvedFork
from profile_curve import Profile
from spine_curve import CubicSpine
from spine_frames import unit
from tapered_experiment import fixtures as tapered_fixtures


def fixtures():
    reference = tapered_fixtures()
    cases = {}
    for name, source, bend in (
        ("bow", reference["symmetric"], (0.15, 0.15)),
        ("side_branch", reference["unequal"], (0.16, -0.16)),
        ("spatial", reference["spatial"], (0.16, 0.08)),
    ):
        spines, profiles = [], []
        for index, (endpoint, base, tip) in enumerate(
            zip(source.endpoints, source.radii, source.tip_radii, strict=True)
        ):
            endpoint = np.asarray(endpoint)
            side = unit(np.cross(endpoint, (0, 0, 1)))
            normal = unit(np.cross(endpoint, side))
            out = 0.12 * (index - 1) if name == "spatial" else 0.0
            controls = np.array(
                (
                    np.zeros(3),
                    endpoint / 3 + bend[0] * side + out * normal,
                    2 * endpoint / 3 + bend[1] * side - out * normal,
                    endpoint,
                )
            )
            spines.append(CubicSpine((controls,)))
            profiles.append(Profile(np.linspace(base, tip, 4)))
        if name == "side_branch":
            parent = CubicSpine(
                (
                    (
                        (0, -0.75, 0),
                        (0.18, -0.25, 0.08),
                        (-0.18, 0.25, -0.08),
                        (0, 0.75, 0),
                    ),
                )
            )
            spines = [parent]
            profiles = [Profile(np.linspace(0.27, 0.045, 4))]
            for attachment, direction, radius in (
                (0.4, (-0.55, 0.4, 0.15), 0.11),
                (0.7, (0.55, 0.4, -0.1), 0.075),
            ):
                anchor, direction = parent.evaluate(attachment), np.asarray(direction)
                side = unit(np.cross(direction, (0, 0, 1)))
                controls = anchor + np.array(
                    (
                        np.zeros(3),
                        direction / 3 + 0.08 * side,
                        2 * direction / 3 - 0.08 * side,
                        direction,
                    )
                )
                spines.append(CubicSpine((controls,)))
                profiles.append(Profile(np.linspace(radius, 0.03, 4)))
        plans = tuple(
            CurveBranch.from_spine(s, p, tolerance=0.001)
            for s, p in zip(spines, profiles, strict=True)
        )
        cases[name] = (CurvedFork(plans, source.blend), spines, profiles)
    return cases


def compare(field, spines, profiles):
    import time
    from functools import partial

    from curved_checks import reference_surface_points, surface_checks
    from fork_checks import mesh_checks
    from fork_surface import blocked_voxel_mesh, voxel_mesh
    from mesh_distance import point_mesh_distances

    probes = reference_surface_points(spines, profiles, field.blend)
    rows, selected, selected_row, reference = [], None, None, None
    methods = (
        partial(voxel_mesh, cells=96),
        partial(blocked_voxel_mesh, cells=64, block_size=8),
        partial(blocked_voxel_mesh, cells=96, block_size=8),
        partial(blocked_voxel_mesh, cells=128, block_size=8),
    )
    for build in methods:
        began = time.perf_counter()
        fresh_spines = [CubicSpine(s.controls) for s in spines]
        fresh_profiles = [Profile(p.table) for p in profiles]
        fresh = CurvedFork(
            tuple(
                CurveBranch.from_spine(s, p, tolerance=0.001)
                for s, p in zip(fresh_spines, fresh_profiles, strict=True)
            ),
            field.blend,
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
        elif work["longest_axis_cells"] == 96:
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], reference[key])
            work["exact_dense_mesh"] = True
        checks = mesh_checks(mesh, field)
        projection = surface_checks(mesh, spines, profiles, field.blend)
        distance = float(np.max(point_mesh_distances(mesh, probes)))
        error = max(distance, projection["maximum_observed_projection_m"])
        passed = (
            error <= 0.005 and checks["faces_disagreeing_with_analytic_normals"] == 0
        )
        row = (
            work
            | checks
            | projection
            | {
                "reference_surface_points": len(probes),
                "maximum_reference_to_mesh_m": distance,
                "maximum_observed_surface_error_m": error,
                "same_quality_pass": passed,
                "median_ms": float(np.median(timings)),
                "samples_ms": timings,
                "first_product_ms": first_ms,
            }
        )
        rows.append(row)
        if passed and (
            selected is None or row["median_ms"] < selected_row["median_ms"]
        ):
            selected, selected_row = mesh, row
    if selected is None:
        raise ValueError("no curved mesh meets both 5 mm surface-probe requirements")
    return selected, {"cases": rows, "selected": selected_row}


def scene(cases, meshes, output, commit, modified):
    import json

    from fork_adaptive import adaptive_mesh
    from organ_experiment import UPRIGHT, write_product

    straight = tapered_fixtures()["symmetric"]
    baseline, _work = adaptive_mesh(straight, 0.004, max_depth=9)
    products = [
        write_product(
            output,
            "straight",
            baseline,
            np.array([[-2.55, 0, 1.02]]),
            UPRIGHT[None],
            "wood",
        )
    ]
    for index, (name, (field, _spines, _profiles)) in enumerate(cases.items()):
        lowest = min(
            p[1] - radius
            for branch in field.branches
            for p, radius in zip(branch.points, branch.radii, strict=True)
        )
        products.append(
            write_product(
                output,
                name,
                meshes[name],
                np.array([[-0.85 + index * 1.7, 0, -lowest]]),
                UPRIGHT[None],
                "wood",
            )
        )
    recipe = {
        "name": "curved-forks",
        "title": "Gerade, gebogene, seitlich angeschlossene und räumliche Astpläne",
        "products": products,
        "camera": [4.8, -12, 4],
        "target": [0, 0, 1],
        "fov_y": 31,
        "ground_size_m": 50,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "scope": "separate spatial cubic branch plans; side branches anchored at continuous parent parameters 0.4 and 0.7. Bounded chord source approximations and complete shared surfaces; original Modeler welding, bark and wind remain unproven",
    }
    (output / "curved-forks.json").write_text(json.dumps(recipe, indent=2) + "\n")


def main():
    import argparse
    import hashlib
    import importlib.metadata
    import json
    import resource
    import subprocess
    from pathlib import Path

    from curved_checks import curve_checks, distance_checks, plan_regressions

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/curved")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = fixtures()
    meshes, results = {}, {}
    checks = plan_regressions() | distance_checks()
    for name, (field, spines, profiles) in cases.items():
        meshes[name], result = compare(field, spines, profiles)
        results[name] = result | {
            "checks": curve_checks(field, spines, profiles),
            "segments": [len(b.segments) for b in field.branches],
            "controls_m": [s.controls for s in spines],
            "radius_tables_m": [p.table.tolist() for p in profiles],
        }
        print(
            name,
            result["selected"]["method"],
            result["selected"]["longest_axis_cells"],
            round(result["selected"]["median_ms"]),
            flush=True,
        )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    scene(cases, meshes, args.output, commit, modified)
    report = {
        "commit": commit,
        "source_modified": modified,
        "checks": checks,
        "cases": results,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "packages": {
            n: importlib.metadata.version(n) for n in ("numpy", "scipy", "scikit-image")
        },
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "curved_fork.py",
                "curved_checks.py",
                "curved_experiment.py",
                "mesh_distance.py",
                "profile_curve.py",
                "spine_curve.py",
                "spine_frames.py",
                "fork_field.py",
                "fork_surface.py",
                "fork_volume.py",
                "fork_intervals.py",
                "tapered_fork.py",
                "fork_checks.py",
                "fork_adaptive.py",
            )
        },
        "scope": "shared-machine CPU, three repetitions, fresh source-plan approximation timed separately; fixed continuous-reference surface points for all methods, plus sampled mesh-to-reference projections. Observed two-direction samples, no global Hausdorff guarantee. General curved fields are not assumed origin-star-shaped",
    }
    (args.output / "curved.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
