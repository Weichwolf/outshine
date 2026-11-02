"""Measure joint-material light response and reference convergence on actual recipes."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import time
from functools import partial
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from probe_terrain_material_substrate import optical_row
from relief_shading_reference import direction
from terrain_material_codebook import fit_recipe
from terrain_material_filter import (
    distribution,
    joint_models,
    moment_responses,
    response,
)
from terrain_material_litter import compose
from terrain_material_moments import accumulate
from terrain_material_moments import response as moment_response


def error(expected, actual):
    delta = np.abs(expected - actual)
    return {"rms": float(np.sqrt(np.mean(delta**2))), "max": float(delta.max())}


def compare(capture):
    palette, layout, receipt = load_capture(capture)
    origin = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    lights = np.array(
        [
            direction(angle, azimuth)
            for azimuth in (0, 90, 180, 270)
            for angle in (0, 30, 60, 80)
        ]
    )
    views = np.array([direction(angle, 37) for angle in (0, 50, 80)])
    angle = 0.731
    directions = np.array(
        [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
    )
    records = []
    codebooks = {}
    for row in range(layout["rows"]):
        evaluate = partial(compose, **optical_row(palette, layout, row))
        codebook = fit_recipe(evaluate, origin)
        codebooks[row] = codebook.centres.tolist()
        for footprint in (0.0001, 0.001, 0.01, 0.1, 1.0):
            for anisotropy in (1, 16):
                dx, dy = directions * [[footprint], [footprint / anisotropy]]
                samples = [
                    distribution(evaluate, origin, dx, dy, order)
                    for order in (32, 64, 128)
                ]
                canonical = evaluate(origin[None], dx[None], dy[None])
                canonical[:, 4:] = np.linalg.solve(np.stack((dx, dy)), canonical[0, 4:])
                start = time.perf_counter_ns()
                joint = joint_models(*samples[-1])
                fixed_moments = accumulate(*samples[-1])
                learned_moments = accumulate(*samples[-1], codebook=codebook)
                build_ms = (time.perf_counter_ns() - start) / 1e6
                supports = {name: len(fields) for name, (fields, _) in joint.items()}
                supports["fixed_sector_moments"] = int(
                    np.count_nonzero(fixed_moments[:, 0])
                )
                supports["static_joint_moments"] = int(
                    np.count_nonzero(learned_moments[:, 0])
                )
                for view in views:
                    curves = [
                        response(raw, weights, lights, view) for raw, weights in samples
                    ]
                    raw, weights = samples[-1]
                    candidates = moment_responses(raw, weights, lights, view)
                    candidates.update(
                        {
                            name: response(fields, areas, lights, view)
                            for name, (fields, areas) in joint.items()
                        }
                    )
                    candidates["current_analytic_filter"] = response(
                        canonical, np.ones(1), lights, view
                    )
                    candidates["fixed_sector_moments"] = moment_response(
                        fixed_moments, lights, view
                    )
                    candidates["static_joint_moments"] = moment_response(
                        learned_moments, lights, view
                    )
                    records.append(
                        {
                            "row": row,
                            "footprint_m": footprint,
                            "anisotropy": anisotropy,
                            "candidate_build_ms_python": build_ms,
                            "fixed_sector_bytes_float32": fixed_moments.size * 4,
                            "view": view.tolist(),
                            "reference_32_64": error(curves[1], curves[0]),
                            "reference_64_128": error(curves[2], curves[1]),
                            "reference_peak": float(curves[2].max()),
                            "reference_rgb": curves[2].tolist(),
                            "models": {
                                name: {
                                    "representatives": supports.get(name, 1),
                                    "error": error(curves[2], curve),
                                    "rgb": curve.tolist(),
                                }
                                for name, curve in candidates.items()
                            },
                        }
                    )
    return receipt, records, codebooks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private comparison output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    receipt, records, codebooks = compare(args.capture)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "python": sys.version,
        "numpy": np.__version__,
        "codebooks": codebooks,
        "codebook_training": {
            "origin_m": (
                np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
            ).tolist(),
            "extents_m": [0.0064, 6.4],
            "side": 64,
            "lloyd_iterations": 32,
        },
        "native_commit": receipt["native_commit"],
        "palette": receipt["palette"],
        "capture_receipt_sha256": hashlib.sha256(
            (args.capture / "receipt.json").read_bytes()
        ).hexdigest(),
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "terrain_material_filter.py",
                "terrain_material_moments.py",
                "terrain_material_codebook.py",
                "terrain_material_litter.py",
                "terrain_material_substrate.py",
                "terrain_material_plane.py",
                "asphalt_field.py",
                "relief_shading_reference.py",
            )
        },
        "limitations": [
            "Controlled metric patches, not measured native fragment positions.",
            "Point substrate/litter material; no class/rock blend, native sky or displaced visibility.",
            "Quadrature differences estimate convergence, not a proven integration error bound.",
            "Existing one-lobe moments and bounded joint PCA representatives; not exact GGX convolution or the LEAN Beckmann model.",
            "Fixed-sector moments use ideal footprint integration; no bounded spatial query or native persistent format is qualified.",
            "No asset format, construction/runtime speed or native frame-time acceptance.",
        ],
        "records": records,
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "comparison.json")


if __name__ == "__main__":
    main()
