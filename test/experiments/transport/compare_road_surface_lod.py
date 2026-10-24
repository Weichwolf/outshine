"""Direct upper-display recipes versus fixed native upper surfaces (#964)."""

import argparse
import hashlib
import json
import lzma
import platform
import resource
import subprocess
import time
from itertools import pairwise
from pathlib import Path

import numpy as np
from road_surface_checks import metadata_error, native_positions, observed_error
from road_surface_lod import envelope, prepare
from road_surface_model import Curve


def check(row, error_m):
    recipe = {key: row[key] for key in ("range_m", "section_m", "seams", "intervals")}
    began = time.perf_counter()
    curve = Curve(recipe)
    plan = prepare(curve, error_m)
    generation = time.perf_counter() - began
    before = native_positions(row)
    stations = np.array(row["stations_m"])
    model_error = float(np.linalg.norm(curve.at(stations) - before, axis=2).max())
    center_error, direction_error = metadata_error(curve)
    assert center_error <= 1e-7 and direction_error <= 1e-8
    candidate_error, witness, tested = observed_error(
        curve, plan["stations_m"], plan["positions_enh_m"]
    )
    assert candidate_error <= plan["maximum_stored_bound_m"] + 1e-7
    baseline_error, baseline_witness, baseline_tested = observed_error(
        curve, stations, before
    )
    baseline_bound = max(
        envelope(curve, a, b, before[i : i + 2])[0]
        for i, (a, b) in enumerate(pairwise(stations))
    )
    bands = 1 if curve.shoulder == 0 else 3
    report = {
        "ordinal": row["ordinal"],
        "form": row["form"],
        "accepted": True,
        "native_stations": len(stations),
        "candidate_stations": len(plan["stations_m"]),
        "native_triangles": (len(stations) - 1) * 2 * bands,
        "candidate_triangles": len(plan["indices"]),
        "candidate_position_bytes": plan["positions_enh_m"].nbytes,
        "candidate_index_bytes": plan["indices"].nbytes,
        "native_upper_position_bytes": before.nbytes,
        "native_upper_index_bytes": (len(stations) - 1) * 2 * bands * 3 * 4,
        "maximum_model_bound_m": plan["maximum_model_bound_m"],
        "maximum_stored_bound_m": plan["maximum_stored_bound_m"],
        "maximum_quantization_m": plan["maximum_vertex_quantization_m"],
        "candidate_observed_error_m": candidate_error,
        "candidate_witness": witness,
        "native_observed_error_m": baseline_error,
        "native_witness": baseline_witness,
        "native_model_bound_m": baseline_bound,
        "native_sample_reconstruction_error_m": model_error,
        "metadata_midpoint_error_m": center_error,
        "metadata_midpoint_direction_error": direction_error,
        "generation_seconds": generation,
        "pose_evaluations": plan["pose_evaluations"],
        "envelope_pose_tests": plan["envelope_pose_tests"],
        "offline_candidate_sample_stations": tested,
        "offline_native_sample_stations": baseline_tested,
        "recipe_json_bytes": len(json.dumps(recipe, separators=(",", ":")).encode()),
    }
    return report, {
        key: plan[key] for key in ("stations_m", "positions_enh_m", "indices")
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--error-m", type=float, default=0.25)
    args = parser.parse_args()
    folder = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/road_curve_requests"
    )
    manifest = json.loads((folder / "manifest.json").read_text())
    archive = folder / manifest["archive"]
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    assert digest == manifest["archive_sha256"]
    rows, arrays = [], {}
    with lzma.open(archive, "rt") as stream:
        for line in stream:
            row = json.loads(line)
            try:
                result, packed = check(row, args.error_m)
                arrays.update(
                    {
                        f"curve_{row['ordinal']}_{key}": value
                        for key, value in packed.items()
                    }
                )
            except (ValueError, RuntimeError, ArithmeticError, AssertionError) as error:
                result = {
                    "ordinal": row["ordinal"],
                    "accepted": False,
                    "error": type(error).__name__ + ": " + str(error),
                }
            rows.append(result)
    assert len(rows) == manifest["records"]
    sources = [
        Path(__file__).with_name(name)
        for name in (
            "road_surface_model.py",
            "road_surface_lod.py",
            "road_surface_checks.py",
            "compare_road_surface_lod.py",
            "test_road_surface_lod.py",
        )
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "archive_sha256": digest,
        "platform": platform.platform(),
        "error_m": args.error_m,
        "whole_process_peak_rss_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Upper surface only, analytic profile model, source metadata as generation input; native upper words are offline checks. No normals/material/UV/ports/junctions/caps/walls/opaque coverage/device admission. RSS includes Python/imports/source decode/diagnostic plans, not app UMA peak.",
        "rows": rows,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(args.output / "plans.npz", **arrays)
    good = [r for r in rows if r["accepted"]]
    print("Accepted", len(good), "/", len(rows))
    print(
        "Stations",
        sum(r["native_stations"] for r in good),
        "->",
        sum(r["candidate_stations"] for r in good),
    )
    print(
        "Native sampled failures at requested error:",
        sum(r["native_observed_error_m"] > args.error_m for r in good),
    )
    if len(good) != len(rows):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
