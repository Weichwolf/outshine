"""Compare organic fork meshes under equal sampled shape-error requirements."""

import argparse
import hashlib
import importlib.metadata
import json
import resource
import subprocess
import time
from functools import partial
from pathlib import Path

import numpy as np
from fork_adaptive import adaptive_mesh
from fork_checks import (
    blend_checks,
    field_checks,
    mesh_checks,
    ray_mesh_radii,
    rejection_checks,
    sphere_checks,
)
from fork_field import ForkField
from fork_surface import radial_mesh, voxel_mesh
from organ_experiment import UPRIGHT, write_product
from spine_frames import unit


def fixtures():
    return {
        "symmetric": ForkField(
            ((0, -0.75, 0), (-0.58, 0.58, 0), (0.58, 0.58, 0)),
            (0.19, 0.13, 0.13),
            0.035,
        ),
        "unequal": ForkField(
            ((0, -0.75, 0), (-0.55, 0.62, 0), (0.65, 0.4, 0)),
            (0.20, 0.155, 0.09),
            0.035,
        ),
        "spatial": ForkField(
            ((0, -0.75, 0), (-0.5, 0.6, 0.35), (0.6, 0.65, 0.15)),
            (0.19, 0.135, 0.12),
            0.035,
        ),
    }


def probe_directions(field):
    random = unit(np.random.default_rng(913).normal(size=(256, 3)))
    probes = [random, unit(field.endpoints)]
    for endpoint, radius in zip(field.endpoints, field.radii, strict=True):
        endpoint = np.asarray(endpoint)
        tangent = unit(endpoint)
        basis = unit(np.cross(tangent, np.eye(3)[np.argmin(np.abs(tangent))]))
        second = np.cross(tangent, basis)
        angle = np.linspace(0, 2 * np.pi, 32, endpoint=False)
        probes.append(
            unit(
                endpoint
                + radius
                * (np.cos(angle)[:, None] * basis + np.sin(angle)[:, None] * second)
            )
        )
    return np.vstack(probes)


def measure(field, method, probes, exact):
    began = time.perf_counter()
    fresh = ForkField(field.endpoints, field.radii, field.blend, field.origin)
    mesh, work = method.func(fresh, *method.args[1:], **method.keywords)
    first_ms = (time.perf_counter() - began) * 1000
    timings = []
    for _ in range(3):
        began = time.perf_counter()
        repeated, _work = method()
        timings.append((time.perf_counter() - began) * 1000)
        for key in mesh:
            np.testing.assert_array_equal(mesh[key], repeated[key])
    checks = mesh_checks(mesh, field)
    approximate = ray_mesh_radii(mesh, probes, field.origin)
    error = float(np.max(np.abs(approximate - exact)))
    return mesh, work | checks | {
        "first_product_ms": first_ms,
        "samples_ms": timings,
        "median_ms": float(np.median(timings)),
        "independent_probe_count": len(probes),
        "maximum_radial_probe_error_m": error,
        "same_quality_pass": error <= 0.005
        and work.get("patch_probe_tolerance_met", True)
        and checks["faces_disagreeing_with_analytic_normals"] == 0,
    }


def compare(field):
    directions = probe_directions(field)
    exact, _stats = field.radial_radius(directions)
    rows, winner_mesh, winner = [], None, None
    methods = (
        *(partial(radial_mesh, field, level) for level in (4, 5, 6)),
        *(partial(voxel_mesh, field, cells) for cells in (32, 64, 96, 128)),
        *(partial(adaptive_mesh, field, tolerance) for tolerance in (0.007, 0.003)),
    )
    for method in methods:
        mesh, row = measure(field, method, directions, exact)
        rows.append(row)
        if row["same_quality_pass"] and (
            winner is None or row["median_ms"] < winner["median_ms"]
        ):
            winner_mesh, winner = mesh, row
    if winner is None:
        raise ValueError("no procedure meets the common sampled shape requirement")
    return winner_mesh, {
        "cases": rows,
        "selected": winner,
        "probe_error_limit_m": 0.005,
        "scope": "same 355 independent random/arm/tip rays; observed radial error only, no global Hausdorff bound",
    }


def scene(fields, selected, output, commit, modified):
    products = []
    common = fields["symmetric"]
    for index, (endpoint, radius) in enumerate(
        zip(common.endpoints, common.radii, strict=True)
    ):
        primitive = ForkField((endpoint,), (radius,), 0)
        mesh, _work = voxel_mesh(primitive, 96)
        products.append(
            write_product(
                output,
                f"overlap-{index}",
                mesh,
                np.array([[-2.55, 0, 0.94]]),
                UPRIGHT[None],
                "wood",
            )
        )
    for index, (name, mesh) in enumerate(selected.items()):
        products.append(
            write_product(
                output,
                name,
                mesh,
                np.array([[-0.85 + index * 1.7, 0, 0.75 + fields[name].radii[0]]]),
                UPRIGHT[None],
                "wood",
            )
        )
    item = {
        "name": "organic-forks",
        "title": "Astgabeln: überlappende Einzeläste, gemeinsame symmetrische, ungleiche und räumliche Oberfläche",
        "products": products,
        "camera": [4.8, -12, 4.0],
        "target": [0, 0, 1.0],
        "fov_y": 31,
        "ground_size_m": 50,
        "resolution": [1280, 720],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "scope": "constant-radius origin-connected segment fixtures; overlap and symmetric union share identical segments/radii, camera and lighting. Closed shared surfaces before full curved-parent welding, bark, wind and plant recipes; no claim of original Modeler scene equivalence",
    }
    (output / "organic-forks.json").write_text(json.dumps(item, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/forks")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fields = fixtures()
    checks = blend_checks() | sphere_checks()
    checks["rejected_invalid_inputs"] = rejection_checks()
    selected, results = {}, {}
    for name, field in fields.items():
        selected[name], comparison = compare(field)
        results[name] = {
            "field_checks": field_checks(field),
            "comparison": comparison,
            "inputs": {
                "endpoints_m": field.endpoints,
                "radii_m": field.radii,
                "blend_m2": field.blend,
            },
        }
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    scene(fields, selected, args.output, commit, modified)
    result = {
        "checks": checks,
        "cases": results,
        "commit": commit,
        "source_modified": modified,
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "fork_field.py",
                "fork_surface.py",
                "fork_adaptive.py",
                "fork_checks.py",
                "fork_experiment.py",
                "cube_surface.py",
            )
        },
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "packages": {
            name: importlib.metadata.version(name)
            for name in ("numpy", "scipy", "scikit-image")
        },
        "scope": "shared-machine CPU, three warmed repetitions; first product separately includes a fresh validated field plan and segment-data activation, library imports excluded. Same segment parameters, compact symmetric field, analytic normal recipe and independent radial shape-error samples. Full mesh generation timed, validation outside timed block. Actual original Weld functions not executed; Ghidra/static primary sources only, no Modeler/native runtime/device cost comparison",
    }
    (args.output / "forks.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "checks": checks,
                "selected": {
                    name: case["comparison"]["selected"]
                    for name, case in results.items()
                },
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
