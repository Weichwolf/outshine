"""Compare spatial frame procedures and produce controlled profiled branch fixtures."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from functools import partial
from pathlib import Path

import numpy as np
from organ_experiment import UPRIGHT, write_product
from profile_curve import BezierCurve, Profile
from profile_experiment import CURVES
from spine_checks import (
    curve_check,
    frame_checks,
    helix,
    mesh_check,
    native_frame_check,
    rejection_checks,
    surface_check,
)
from spine_curve import CubicSpine
from spine_frames import reflection_loop, transport_frames, unit
from spine_surface import spine_mesh


def fixtures():
    parameter = np.linspace(0, 1, 9)
    zeros = np.zeros_like(parameter)
    angle = 2 * np.pi * parameter
    inflection = np.column_stack(
        (zeros, 1.8 * parameter, 1.1 * ((parameter - 0.5) ** 3 + 0.125))
    )
    inflection_d = np.column_stack(
        (zeros, np.full_like(parameter, 1.8), 3.3 * (parameter - 0.5) ** 2)
    )
    space = np.column_stack(
        (0.23 * np.sin(angle), 1.8 * parameter, 0.17 * (1 - np.cos(angle)))
    )
    space_d = np.column_stack(
        (
            0.23 * 2 * np.pi * np.cos(angle),
            np.full_like(parameter, 1.8),
            0.17 * 2 * np.pi * np.sin(angle),
        )
    )
    curl, curl_d, _reference = helix(parameter)
    return {
        "inflection": (CubicSpine.from_hermite(inflection, inflection_d), (0, 0)),
        "spatial": (CubicSpine.from_hermite(space, space_d), (0, 0)),
        "curl": (CubicSpine.from_hermite(curl, curl_d), (0, 0)),
        "rolled": (CubicSpine.from_hermite(space, space_d), (0, 1.5 * np.pi)),
    }


def timed(method, repeats=7):
    result = method()
    samples = []
    for _ in range(repeats):
        began = time.perf_counter()
        method()
        samples.append((time.perf_counter() - began) * 1000)
    return result, {"samples_ms": samples, "median_ms": float(np.median(samples))}


def compare_frames():
    rows = []
    for segments in (16, 32, 64, 128, 256, 512, 1024):
        parameter = np.linspace(0, 1, segments + 1)
        centre, velocity, exact = helix(parameter)
        tangent = unit(velocity)
        for name, method in (
            ("reflection_loop", partial(reflection_loop, centre, tangent)),
            ("reflection", partial(transport_frames, centre, tangent)),
            ("rotation", partial(transport_frames, centre, tangent, method="rotation")),
        ):
            result, cost = timed(method)
            error = np.linalg.norm(result[:, 0] - exact, axis=1)
            rows.append(
                {
                    "method": name,
                    "segments": segments,
                    "maximum_frame_vector_error": float(np.max(error)),
                    "cost": cost,
                }
            )
    acceptable = {}
    for method in ("reflection_loop", "reflection", "rotation"):
        candidates = [
            row
            for row in rows
            if row["method"] == method and row["maximum_frame_vector_error"] <= 1e-5
        ]
        assert candidates
        acceptable[method] = min(candidates, key=lambda row: row["cost"]["median_ms"])
    return {
        "equal_sample_cases": rows,
        "same_quality_limit_vector_error": 1e-5,
        "fastest_passing_case": acceptable,
    }


def check_detail(spine, profile, roll):
    meshes = {}
    rows = []
    for segments, sides in ((16, 8), (64, 16), (128, 32)):
        began = time.perf_counter()
        fresh_spine = CubicSpine(spine.controls)
        fresh_profile = Profile.from_curve(BezierCurve(CURVES["linear"]))
        first = spine_mesh(
            fresh_spine,
            fresh_profile,
            0.13,
            segments,
            sides,
            ellipse=(1, 0.55),
            roll=roll,
        )
        first_product_ms = (time.perf_counter() - began) * 1000
        mesh, cost = timed(
            partial(
                spine_mesh,
                spine,
                profile,
                0.13,
                segments,
                sides,
                ellipse=(1, 0.55),
                roll=roll,
            )
        )
        for key in mesh:
            np.testing.assert_array_equal(first[key], mesh[key])
        meshes[segments] = mesh
        mesh_check(mesh)
        rows.append(
            {
                "segments": segments,
                "sides": sides,
                "triangles": len(mesh["faces"]),
                "product_bytes": sum(array.nbytes for array in mesh.values()),
                "first_product_from_fresh_plan_and_profile_ms": first_product_ms,
                "cost": cost,
            }
        )
    coarse = meshes[16]["vertices"][:-2].reshape(17, 8, 3).mean(axis=1)
    fine = meshes[128]["vertices"][:-2].reshape(129, 32, 3).mean(axis=1)[::8]
    np.testing.assert_allclose(coarse, fine, atol=2e-14)
    return rows


def scene(cases, profile, output, commit, modified):
    products = []
    for index, (name, (spine, roll)) in enumerate(cases.items()):
        mesh = spine_mesh(spine, profile, 0.13, 128, 32, ellipse=(1, 0.55), roll=roll)
        products.append(
            write_product(
                output,
                name,
                mesh,
                np.array([[(index - 1.5) * 0.8, 0, 0]]),
                UPRIGHT[None],
                "wood",
            )
        )
    item = {
        "name": "spatial-branches",
        "title": "Räumliche Astformen: Wendepunkt, S-Verlauf, Curl und gerollter Querschnitt",
        "products": products,
        "camera": [3.2, -6.5, 3.4],
        "target": [0, 0, 0.95],
        "fov_y": 30,
        "ground_size_m": 40,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "scope": "authored 3D spines, common radius profile and elliptical cross-section; functional fixtures before botanical recipes; no branch welding, bark, wind or claim of full SpeedTree Spine equivalence",
    }
    (output / "spatial-branches.json").write_text(json.dumps(item, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/spines")
    )
    parser.add_argument("--oracle", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    cases = fixtures()
    profile = Profile.from_curve(BezierCurve(CURVES["linear"]))
    preparation_ms = (time.perf_counter() - began) * 1000
    checks = frame_checks()
    checks["native_reference"] = native_frame_check(args.oracle)
    checks["rejected_invalid_cases"] = rejection_checks(cases["curl"][0], profile)
    results = {}
    for name, (spine, roll) in cases.items():
        results[name] = {
            "curve": curve_check(spine),
            "surface": surface_check(spine, profile, roll),
            "direct_detail": check_detail(spine, profile, roll),
        }
    comparison = compare_frames()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    scene(cases, profile, args.output, commit, modified)
    result = {
        "checks": checks,
        "cases": results,
        "comparison": comparison,
        "curve_and_profile_preparation_ms": preparation_ms,
        "commit": commit,
        "source_modified": modified,
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "spine_curve.py",
                "spine_frames.py",
                "spine_surface.py",
                "tube_topology.py",
                "spine_checks.py",
                "spine_experiment.py",
            )
        },
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "local shared-machine CPU; seven warmed repeats with identical curve inputs/start frame. First-product time separately includes fresh curve construction, lazy BPoly activation and complete profile preparation from authored controls. Same-quality comparison requires error <=1e-5 against analytic helix. Direct coarse sweep does not build fine geometry. OrthoNormalize native oracle only, no original full Spine/cost/image comparison",
    }
    (args.output / "spines.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "checks": checks,
                "curve_errors": {name: item["curve"] for name, item in results.items()},
                "same_quality": comparison["fastest_passing_case"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
