"""Finite clear-gas experiment for #206; requires NumPy, an explicit external output and host lock.

Run compare_air_paths.py --output /tmp/outshine-light-air-<commit> under the shared host lock.
Then test_air_path_integral.py checks independent closed forms; plot_air_paths.py renders diagnostics.
View/source optical-depth integration is shared by the finite Gauss reference and the
optimistic distant-sky baseline. Timings describe this CPU experiment, not LUT-backed GPU work.
Multiple scattering and current Sky-View sampling are outside this first isolated model.
"""

import argparse
import hashlib
import json
import platform
import time
from pathlib import Path

import numpy as np
from air_path_cases import place_cases, usable_distances
from air_path_integral import (
    distant_sky,
    engine_clear_air,
    gauss_path,
    midpoint_path,
    quadrature,
)

ROOT = Path(__file__).resolve().parents[3]
COUNTS = (4, 8, 16, 32, 64)
RADIANCE_FLOOR_PER_SR = 1e-5


def evaluate(medium, case, distance, sky):
    arguments = (medium, *case.arguments(), distance)
    reference, reference_t = gauss_path(*arguments, 512, optical_samples=128)
    refined, refined_t = gauss_path(*arguments, 1024, optical_samples=256)
    candidates = {"sky_times_loss": (sky * (1.0 - reference_t), reference_t)}
    elapsed = {}
    for name, method in [("midpoint", midpoint_path), ("gauss", gauss_path)]:
        for count in COUNTS:
            key = f"{name}_{count}"
            began = time.perf_counter()
            result = method(*arguments, count)
            elapsed[key] = time.perf_counter() - began
            candidates[key] = result
    scale = np.maximum(reference, RADIANCE_FLOOR_PER_SR)
    errors = {}
    for name, (radiance, through) in candidates.items():
        errors[name] = {
            "radiance_rgb_per_sr": radiance.tolist(),
            "transmittance_rgb": through.tolist(),
            "radiance_relative_max": float(
                np.max(np.abs(radiance - reference) / scale)
            ),
            "transmittance_absolute_max": float(np.max(np.abs(through - reference_t))),
            "cpu_seconds": elapsed.get(name),
        }
    return {
        "case": case.name,
        "distance_km": float(distance),
        "reference_rgb_per_sr": reference.tolist(),
        "reference_t_rgb": reference_t.tolist(),
        "reference_refinement_relative_max": float(
            np.max(np.abs(refined - reference) / scale)
        ),
        "reference_refinement_t_max": float(np.max(np.abs(refined_t - reference_t))),
        "candidates": errors,
    }


def aggregate(rows):
    totals = {}
    for name in rows[0]["candidates"]:
        errors = [row["candidates"][name] for row in rows]
        cpu = [row["cpu_seconds"] for row in errors if row["cpu_seconds"] is not None]
        totals[name] = {
            "radiance_relative_p95": float(
                np.percentile([row["radiance_relative_max"] for row in errors], 95)
            ),
            "radiance_relative_max": max(
                row["radiance_relative_max"] for row in errors
            ),
            "transmittance_absolute_max": max(
                row["transmittance_absolute_max"] for row in errors
            ),
            "cpu_seconds_total": sum(cpu) if cpu else None,
        }
    return totals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root, output = args.root.resolve(), args.output.resolve()
    if output == root or root in output.parents:
        raise ValueError("private comparison outputs belong outside the repository")
    medium_path = root / "src/world/sky/Atmosphere.h"
    medium = engine_clear_air(medium_path)
    cases, inputs = place_cases(root)
    began = time.perf_counter()
    for count in [*COUNTS, 128, 256, 512, 1024]:
        quadrature(count)
    setup_seconds = time.perf_counter() - began
    rows = []
    for case in cases:
        sky = distant_sky(medium, *case.arguments(), samples=512, optical_samples=128)
        rows.extend(
            evaluate(medium, case, distance, sky)
            for distance in usable_distances(medium, case)
        )
    totals = aggregate(rows)
    refinement = max(row["reference_refinement_relative_max"] for row in rows)
    report = {
        "scope": "Single Rayleigh scattering plus ozone; Haze=0; no multiple scattering, "
        "clouds, terrain shadows, ground bounce, finite Sky-View LUT or runtime proof.",
        "baseline": "Optimistic analytic distant sky times finite-segment loss, "
        "isolating the heuristic after #194 rather than reproducing LUT errors.",
        "units": {
            "radiance": "radiance / solar illuminance, sr^-1",
            "transmittance": "unitless RGB",
            "cpu": "seconds, isolated process",
        },
        "host": platform.platform(),
        "numpy": np.__version__,
        "reference_view_nodes": 512,
        "refined_view_nodes": 1024,
        "candidate_optical_depth_nodes": 64,
        "reference_optical_depth_nodes": 128,
        "refined_optical_depth_nodes": 256,
        "radiance_relative_floor_per_sr": RADIANCE_FLOOR_PER_SR,
        "quadrature_setup_seconds": setup_seconds,
        "maximum_reference_refinement_relative_error": refinement,
        "reference_passes_0_1_percent": refinement <= 0.001,
        "requirements": {
            "owner": "outshine_light",
            "origin": "pre-measurement experiment target, not a renderer gate",
            "radiance_relative_max": 0.01,
            "transmittance_absolute_max": 0.002,
        },
        "meeting_requirements": [
            name
            for name, row in totals.items()
            if refinement <= 0.001
            and row["radiance_relative_max"] <= 0.01
            and row["transmittance_absolute_max"] <= 0.002
        ],
        "cases": [case.record() for case in cases],
        "summary": totals,
        "results": rows,
        "inputs_sha256": {
            str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [medium_path, root / "src/world/sky/Ephemeris.h", *inputs]
        },
        "sources_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in Path(__file__).parent.glob("*air_path*.py")
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "rays": len(rows),
                "reference_refinement_max": refinement,
                "meeting_requirements": report["meeting_requirements"],
                "summary": totals,
            }
        )
    )


if __name__ == "__main__":
    main()
