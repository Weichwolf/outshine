"""Same-source coupled road fallback versus always-coupled planning (#1539)."""

import argparse
import collections
import hashlib
import json
import os
import platform
import resource
import subprocess
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from compare_road_bank import diagnose, load
from road_bank_profile import BankLimits, prior_controls
from road_joint_profile import plan_joint
from road_shared_profile import ProfileRecipe


def run(
    cases,
    limits,
    height_change,
    rounds,
    *,
    methods=("staged", "coupled"),
    planner=plan_joint,
    witnesses=(7977, 666),
):
    rows, arrays = [], {}
    for index, (source, curve) in enumerate(cases):
        row = {"ordinal": source["ordinal"], "methods": {}}
        for repeat in range(rounds):
            order = methods if (index + repeat) % 2 == 0 else methods[::-1]
            for method in order:
                started = time.process_time_ns()
                value, note = planner(curve, limits, height_change, method=method)
                elapsed = time.process_time_ns() - started
                if repeat == 0:
                    note["cpu_ns"] = [elapsed]
                    row["methods"][method] = note
                    if value is not None:
                        h, q = (
                            (value.height, value.bank)
                            if isinstance(value, ProfileRecipe)
                            else value
                        )
                        arrays[f"curve_{source['ordinal']}_{method}_height"] = h
                        arrays[f"curve_{source['ordinal']}_{method}_bank"] = q
                        if method == methods[0]:
                            row["geometry"] = diagnose(
                                source,
                                curve,
                                q,
                                arrays,
                                height_controls=h,
                                witnesses=witnesses,
                                surface=value.surface(source)
                                if isinstance(value, ProfileRecipe)
                                else None,
                            )
                            assert (
                                row["geometry"]["corrected_sampled_maximum_grade"]
                                <= limits.grade + 1e-8
                            )
                            if note["selected"] in ("bank_only", "unchanged_quadratic"):
                                np.testing.assert_array_equal(h, curve.height)
                            if note["selected"] == "unchanged_quadratic":
                                np.testing.assert_array_equal(q, prior_controls(curve))
                else:
                    old = row["methods"][method]
                    assert old["status"] == note["status"] and old.get(
                        "reason"
                    ) == note.get("reason")
                    if value is not None:
                        coefficients = (
                            (value.height, value.bank)
                            if isinstance(value, ProfileRecipe)
                            else value
                        )
                        for field, a in zip(
                            ("height", "bank"), coefficients, strict=True
                        ):
                            np.testing.assert_array_equal(
                                a, arrays[f"curve_{source['ordinal']}_{method}_{field}"]
                            )
                    old["cpu_ns"].append(elapsed)
        rows.append(row)
    return rows, arrays


def traces(
    cases,
    limits,
    height_change,
    output,
    *,
    methods=("staged", "coupled"),
    planner=plan_joint,
):
    import memray
    from replay_canonical_bridge_ports import allocations

    result = {}
    for method in methods:
        file = output / (method + ".bin")
        with memray.Tracker(str(file), trace_python_allocators=True):
            for _, curve in cases:
                planner(curve, limits, height_change, method=method)
        result[method] = allocations(file)
        result[method]["scope"] = (
            "Warm prepared-input planning only, including bank-only fallback for staged. No diagnostic/geometry retention, copied-byte, runtime-UMA or device claim."
        )
    return result


def metrics(rows, arrays, methods, rounds):
    return {
        "counts": {
            m: dict(
                collections.Counter(
                    row["methods"][m].get("selected", row["methods"][m].get("reason"))
                    for row in rows
                )
            )
            for m in methods
        },
        "cpu_ns": {
            m: [
                sum(row["methods"][m]["cpu_ns"][n] for row in rows)
                for n in range(rounds)
            ]
            for m in methods
        },
        "lp_calls": {
            m: sum(
                row["methods"][m]["coupled_lp_calls"]
                + row["methods"][m]["base_lp_calls"]
                for row in rows
            )
            for m in methods
        },
        "lp_rows": {
            m: sum(
                row["methods"][m].get("inequalities", 0)
                + row["methods"][m].get("base", {}).get("inequalities", 0)
                for row in rows
            )
            for m in methods
        },
        "retained_control_bytes": {
            m: sum(a.nbytes for k, a in arrays.items() if "_" + m + "_" in k)
            for m in methods
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    for arg in ("grade", "crossfall", "change", "height-change"):
        parser.add_argument("--" + arg, type=float, required=True)
    parser.add_argument("--rounds", type=int, choices=(1, 3), default=1)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    limits = BankLimits(args.grade, args.crossfall, args.change)
    cases, digest = load()
    rows, arrays = run(cases, limits, args.height_change, args.rounds)
    loop_peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    allocation_report = (
        traces(cases, limits, args.height_change, args.output) if args.trace else {}
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "platform": platform.platform(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "source_sha256": digest,
        "explicit_experiment_limits_not_source_facts": asdict(limits)
        | {"height_change_m": args.height_change},
        "source_hashes": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "compare_road_joint.py",
                "road_joint_profile.py",
                "road_bank_profile.py",
                "road_banked_surface.py",
                "compare_road_bank.py",
            )
        },
        **metrics(rows, arrays, ("staged", "coupled"), args.rounds),
        "loop_peak_rss_bytes": loop_peak,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": allocation_report,
        "rows": rows,
        "scope": "Same fixed real isolated sweeps/outer profiles; G/Q/D/H are explicit experiment inputs. Original interval basis, no extra knots; conservative LP family and numerical C1/residual rejection remain. Distinct ready/fidelity sets reported, no shared global-optimality, terrain/clearance/Source/whole-native-body/network/world/device claim. RSS includes input and retained controls/diagnostics.",
    }
    np.savez_compressed(args.output / "profiles.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: report[key]
                for key in (
                    "counts",
                    "cpu_ns",
                    "lp_calls",
                    "lp_rows",
                    "retained_control_bytes",
                    "loop_peak_rss_bytes",
                    "allocations",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
