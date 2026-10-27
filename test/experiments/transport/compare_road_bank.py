"""Actual fixed-source bank corrections and equal-bound method work (#1533)."""

import argparse
import collections
import hashlib
import json
import lzma
import os
import platform
import resource
import subprocess
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from road_bank_profile import BankLimits, plan_bank
from road_banked_surface import BankedSurface
from road_surface_frames import differentials
from road_surface_lod import prepare
from road_surface_model import Curve


def sample_grade(curve, stations):
    ds, _ = differentials(curve, stations)
    return abs(ds[:, :, 2]) / np.linalg.norm(ds[:, :, :2], axis=2)


def diagnose(
    row, curve, control, arrays, *, height_controls=None, witnesses=(4013, 55171)
):
    fitted = BankedSurface(row, control, height_controls=height_controls)
    stations = np.unique(
        np.r_[
            np.linspace(*curve.stations[[0, -1]], 1001),
            np.concatenate(
                [
                    np.linspace(a, b, 17)
                    for a, b in zip(
                        curve.stations[:-1], curve.stations[1:], strict=True
                    )
                ]
            ),
        ]
    )
    old_grade, new_grade = sample_grade(curve, stations), sample_grade(fitted, stations)
    original, after = curve.at(stations), fitted.at(stations)
    np.testing.assert_array_equal(original[:, :, :2], after[:, :, :2])
    ends = curve.at(curve.stations[[0, -1]])
    np.testing.assert_allclose(
        ends, fitted.at(curve.stations[[0, -1]]), rtol=0, atol=1e-10
    )
    display = {}
    if row["ordinal"] in witnesses:
        plan = prepare(fitted, 0.01)
        display = {
            "display_model_bound_m": plan["maximum_stored_bound_m"],
            "display_triangles": len(plan["indices"]),
        }
        for name, data in {
            "stations": stations,
            "original": original,
            "corrected": after,
            "original_grade": old_grade,
            "corrected_grade": new_grade,
            "display_positions": plan["positions_enh_m"],
            "display_indices": plan["indices"],
        }.items():
            arrays[f"witness_{row['ordinal']}_{name}"] = data
    return {
        "original_sampled_maximum_grade": float(old_grade.max()),
        "corrected_sampled_maximum_grade": float(new_grade.max()),
        "maximum_observed_height_change_m": float(abs(original - after).max()),
        "diagnostic_stations": len(stations),
        **display,
    }


def load():
    root = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/road_curve_requests"
    )
    manifest = json.loads((root / "manifest.json").read_text())
    archive = root / manifest["archive"]
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    assert digest == manifest["archive_sha256"]
    with lzma.open(archive, "rt") as stream:
        rows = [json.loads(line) for line in stream]
    assert len(rows) == manifest["records"]
    # Explicitly strip emitted geometry/normals before constructing any curve.
    cases = [
        (r, Curve({k: r[k] for k in ("range_m", "section_m", "seams", "intervals")}))
        for r in rows
    ]
    return cases, digest


def run(cases, limits, rounds):
    rows, arrays = [], {}
    for index, (row, curve) in enumerate(cases):
        result = {"ordinal": row["ordinal"], "methods": {}}
        for repeat in range(rounds):
            methods = (
                ("adaptive", "linear")
                if (index + repeat) % 2 == 0
                else ("linear", "adaptive")
            )
            for method in methods:
                began = time.process_time_ns()
                value, note = plan_bank(curve, limits, method=method)
                elapsed = time.process_time_ns() - began
                if repeat == 0:
                    note["cpu_ns"] = [elapsed]
                    result["methods"][method] = note
                    if value is not None:
                        arrays[f"curve_{row['ordinal']}_{method}"] = value
                    if method == "adaptive" and value is not None:
                        result["geometry"] = diagnose(row, curve, value, arrays)
                        assert (
                            result["geometry"]["corrected_sampled_maximum_grade"]
                            <= limits.grade + 1e-8
                        )
                else:
                    old = result["methods"][method]
                    assert (
                        old["status"] == note["status"]
                        and old["reason"] == note["reason"]
                    )
                    if value is not None:
                        np.testing.assert_array_equal(
                            value, arrays[f"curve_{row['ordinal']}_{method}"]
                        )
                    old["cpu_ns"].append(elapsed)
        rows.append(result)
    return rows, arrays


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--grade", type=float, required=True)
    parser.add_argument("--crossfall", type=float, required=True)
    parser.add_argument("--change", type=float, required=True)
    parser.add_argument("--rounds", type=int, choices=(1, 3), default=1)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    limits = BankLimits(args.grade, args.crossfall, args.change)
    cases, digest = load()
    rows, arrays = run(cases, limits, args.rounds)
    peak_before_metadata = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    allocation_report = trace_planning(cases, limits, args.output) if args.trace else {}
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "platform": platform.platform(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "source_sha256": digest,
        "explicit_experiment_limits_not_source_facts": asdict(limits),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__),
                *[
                    Path(__file__).with_name(n)
                    for n in (
                        "road_bank_profile.py",
                        "road_banked_surface.py",
                        "road_surface_model.py",
                        "road_surface_frames.py",
                    )
                ],
            ]
        },
        "counts": {
            m: dict(
                collections.Counter(
                    r["methods"][m].get("selected", r["methods"][m]["reason"])
                    for r in rows
                )
            )
            for m in ("adaptive", "linear")
        },
        "cpu_ns": {
            m: [
                sum(r["methods"][m]["cpu_ns"][n] for r in rows)
                for n in range(args.rounds)
            ]
            for m in ("adaptive", "linear")
        },
        "lp_calls": {
            m: sum(r["methods"][m]["lp_calls"] for r in rows)
            for m in ("adaptive", "linear")
        },
        "lp_inequalities": {
            m: sum(r["methods"][m].get("inequalities", 0) for r in rows)
            for m in ("adaptive", "linear")
        },
        "retained_control_bytes": {
            m: sum(data.nbytes for k, data in arrays.items() if k.endswith("_" + m))
            for m in ("adaptive", "linear")
        },
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "loop_peak_rss_before_metadata_bytes": peak_before_metadata,
        "planning_allocations": allocation_report,
        "scope": "Isolated real pre-port sweeps; explicit test limits, not source classes. Whole Bernstein conditions are sufficient/conservative; sampled grades are witnesses. Distinct ready sets and control fidelity reported; no shared optimum, world/network/vehicle/device claim. RSS includes original source decode, model/control/diagnostic retention.",
        "rows": rows,
    }
    np.savez_compressed(args.output / "profiles.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "counts",
                    "cpu_ns",
                    "lp_calls",
                    "lp_inequalities",
                    "retained_control_bytes",
                    "process_peak_rss_bytes",
                )
            }
        )
    )


def trace_planning(cases, limits, output):
    import memray
    from replay_canonical_bridge_ports import allocations

    result = {}
    for method in ("adaptive", "linear"):
        path = output / (method + ".bin")
        with memray.Tracker(str(path), trace_python_allocators=True):
            for _, curve in cases:
                plan_bank(curve, limits, method=method)
        result[method] = allocations(path)
        result[method]["scope"] = (
            "Warm planning only; prepared curve input and warm solver imports excluded. No geometry/diagnostic retention, runtime UMA or copied-byte claim."
        )
    return result


if __name__ == "__main__":
    main()
