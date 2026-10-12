"""Measure profile preparation/lookup and render controlled branch-taper fixtures."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from axis_surface import profiled_axis_mesh
from organ_experiment import UPRIGHT, write_product
from profile_checks import check_curves, rejection_checks, surface_checks
from profile_curve import BezierCurve, Profile

CURVES = {
    "linear": (((0, 1), (1 / 3, 0.675), (2 / 3, 0.35), (1, 0.025)),),
    "droop": (((0, 1), (0.15, 0.26), (0.7, 0.18), (1, 0.025)),),
    "bulge": (
        ((0, 1), (0.1, 0.2), (0.18, 0.10), (0.35, 0.15)),
        ((0.35, 0.15), (0.50, 0.20), (0.63, 0.70), (0.76, 0.65)),
        ((0.76, 0.65), (0.85, 0.58), (0.96, 0.10), (1, 0.025)),
    ),
    "flare": (
        ((0, 1), (0.03, 0.35), (0.12, 0.25), (0.22, 0.23)),
        ((0.22, 0.23), (0.42, 0.20), (0.85, 0.09), (1, 0.025)),
    ),
}


def measure(curve):
    began = time.perf_counter()
    profile = Profile.from_curve(curve)
    preparation_ms = (time.perf_counter() - began) * 1000
    queries = np.random.default_rng(17).uniform(0, 1, 2048)
    exact = curve.reference_value(queries)
    np.testing.assert_allclose(curve.value(queries), exact, atol=1e-11)
    actual = profile.evaluate(queries)
    samples = {}
    for name, method in (
        ("exact", curve.reference_value),
        ("batched_exact", curve.value),
        ("lookup", profile.evaluate),
    ):
        timings = []
        for _ in range(5):
            began = time.perf_counter()
            method(queries)
            timings.append((time.perf_counter() - began) * 1000)
        samples[name] = {"samples_ms": timings, "median_ms": float(np.median(timings))}
    return profile, {
        "preparation_ms": preparation_ms,
        "table_bytes": profile.bytes(),
        "queries": len(queries),
        "maximum_lookup_error": float(np.max(np.abs(actual - exact))),
        "cost": samples,
    }


def scene(profiles, output, commit, modified):
    products = []
    for i, (name, profile) in enumerate(profiles.items()):
        mesh = profiled_axis_mesh(1.8, 0.45, 0.13, profile, segments=128, sides=32)
        products.append(
            write_product(
                output,
                name,
                mesh,
                np.array([[(i - (len(profiles) - 1) / 2) * 0.55, 0, 0]]),
                UPRIGHT[None],
                "wood",
            )
        )
    item = {
        "name": "profile-branches",
        "title": "Radiusprofile: linear, gleitender Verlauf, Verdickung, Wurzelanlauf",
        "products": products,
        "camera": [2.7, -5, 2.6],
        "target": [0, 0, 0.82],
        "fov_y": 28,
        "ground_size_m": 40,
        "resolution": [1280, 720],
        "samples": 32,
        "source_commit": commit,
        "source_modified": modified,
        "scope": "same centreline, base radius, positions, camera and light; own profile function fixtures, not complete botanical recipes",
    }
    (output / "profile-branches.json").write_text(json.dumps(item, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/profiles")
    )
    parser.add_argument("--oracle", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    curves = [BezierCurve(controls) for controls in CURVES.values()]
    checks = check_curves(
        curves + [BezierCurve((((0, 0), (0.8, 0.2), (0.2, 0.8), (1, 1)),))], args.oracle
    )
    checks["rejected_invalid_cases"] = rejection_checks()
    profiles, cases = {}, {}
    for name, curve in zip(CURVES, curves, strict=True):
        profiles[name], cases[name] = measure(curve)
        cases[name]["surface_checks"] = surface_checks(profiles[name])
        np.savez_compressed(
            args.output / f"{name}-profile.npz", table=profiles[name].table
        )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    scene(profiles, args.output, commit, modified)
    result = {
        "checks": checks,
        "cases": cases,
        "commit": commit,
        "source_modified": modified,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "local CPU, five equal query batches; preparation excluded from repeated lookup, native micro-oracle only validates behavior, not Modeler performance",
    }
    (args.output / "profiles.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "checks": {k: v for k, v in checks.items() if k != "native_reference"},
                "cases": cases,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
