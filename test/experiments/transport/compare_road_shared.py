"""Shared scalar cubic versus existing coupled fallback, same source/caps (#1542)."""

import argparse
import hashlib
import json
import os
import platform
import resource
import subprocess
from dataclasses import asdict
from pathlib import Path

import numpy as np
from compare_road_bank import load
from compare_road_joint import metrics, run, traces
from road_bank_profile import BankLimits, prior_controls
from road_joint_profile import plan_joint, plan_profile
from road_shared_profile import ProfileRecipe
from road_surface_frames import differentials, unit
from road_surface_model import bezier

METHODS = ("shared", "staged")


def plan(curve, limits, height_change, *, method):
    if method == "shared":
        value, note = plan_profile(curve, limits, height_change)
    elif method == "staged":
        pair, note = plan_joint(curve, limits, height_change)
        value = None if pair is None else ProfileRecipe("source-cubics", *pair)
    else:
        raise ValueError("Known comparison method required")
    return value, note | {"basis": value.basis if value is not None else None}


def audit_shared(cases, rows, arrays, limits, height_change):
    result = []
    for (source, curve), row in zip(cases, rows, strict=True):
        if row["methods"]["shared"].get("selected") != "shared_cubic":
            continue
        prefix = f"curve_{source['ordinal']}_shared_"
        product = ProfileRecipe(
            "shared-cubic", arrays[prefix + "height"], arrays[prefix + "bank"]
        )
        surface = product.surface(source)
        indices = np.repeat(np.arange(len(curve.lengths)), 33)
        t = np.tile(np.linspace(0, 1, 33), len(curve.lengths))
        dh = abs(surface.height_at(indices, t) - bezier(curve.height[indices], t))
        q = surface.crossfall(indices, t)
        dq = abs(q - bezier(prior_controls(curve)[indices], t))
        assert dh.max() <= height_change + 1e-9
        assert dq.max() <= limits.change + 1e-9
        assert abs(q).max() <= limits.crossfall + 1e-9
        before, after = (
            np.arange(len(curve.lengths) - 1),
            np.arange(1, len(curve.lengths)),
        )
        scalar_jump = 0.0
        for evaluate in (surface.height_at, surface.crossfall):
            for derivative in (0, 1):
                left = evaluate(before, np.ones(len(before)), derivative=derivative)
                right = evaluate(after, np.zeros(len(after)), derivative=derivative)
                scalar_jump = max(scalar_jump, float(abs(left - right).max(initial=0)))
        assert scalar_jump <= 1e-12
        seams = curve.stations[1:-1]
        left, lu = differentials(surface, seams, side="left")
        right, ru = differentials(surface, seams, side="right")
        normals = abs(unit(np.cross(left, lu)) - unit(np.cross(right, ru)))
        result.append(
            {
                "ordinal": source["ordinal"],
                "maximum_sampled_height_change_m": float(dh.max()),
                "maximum_sampled_bank_change": float(dq.max()),
                "maximum_scalar_seam_jump": scalar_jump,
                "maximum_source_axis_normal_component_jump": float(
                    normals.max(initial=0)
                ),
            }
        )
    return result


def preserve_qualified(rows, arrays):
    count = 0
    for row in rows:
        if row["methods"]["staged"]["status"] != "ready":
            continue
        assert row["methods"]["shared"]["status"] == "ready"
        assert row["methods"]["shared"]["basis"] == "source-cubics"
        for field in ("height", "bank"):
            old = arrays[f"curve_{row['ordinal']}_staged_{field}"]
            new = arrays[f"curve_{row['ordinal']}_shared_{field}"]
            assert old.shape == new.shape and old.dtype == new.dtype
            assert old.tobytes() == new.tobytes()
        count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    for arg in ("grade", "crossfall", "change", "height-change"):
        parser.add_argument("--" + arg, type=float, required=True)
    parser.add_argument("--rounds", type=int, choices=(1, 3), default=1)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    cases, digest = load()
    limits = BankLimits(args.grade, args.crossfall, args.change)
    rows, arrays = run(
        cases,
        limits,
        args.height_change,
        args.rounds,
        methods=METHODS,
        planner=plan,
        witnesses=(937, 6576),
    )
    audit = audit_shared(cases, rows, arrays, limits, args.height_change)
    preserved = preserve_qualified(rows, arrays)
    loop_peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    allocation_report = (
        traces(
            cases,
            limits,
            args.height_change,
            args.output,
            methods=METHODS,
            planner=plan,
        )
        if args.trace
        else {}
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": bool(subprocess.check_output(["git", "status", "--porcelain"])),
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
                "compare_road_shared.py",
                "road_shared_profile.py",
                "road_joint_profile.py",
                "road_bank_profile.py",
                "road_surface_model.py",
                "road_surface_frames.py",
                "road_banked_surface.py",
                "compare_road_bank.py",
                "compare_road_joint.py",
            )
        },
        **metrics(rows, arrays, METHODS, args.rounds),
        "loop_peak_rss_bytes": loop_peak,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": allocation_report,
        "shared_scalar_audit": audit,
        "old_qualified_products_byte_equal": preserved,
        "rows": rows,
        "scope": "Same real isolated source axes/outer profiles and explicit G/Q/Dq/H. Shared scalar cubics are an explicit new representation; source-cubics defaults preserved. C1 scalars do not qualify arbitrary source-axis/native-body C1. Input/diagnostic retention included in RSS. No copied-byte, native/World/terrain/clearance/network/device/frame claim.",
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
                    "lp_rows",
                    "retained_control_bytes",
                    "allocations",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
