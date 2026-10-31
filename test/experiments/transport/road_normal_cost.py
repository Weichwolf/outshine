"""Compare frozen whole road-normal producers with the same saved output words.

The explicit producer checkout permits identical measurement code before/after
an optimization. Source decode and output verification are outside generation.
"""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path


def run(args):
    root = args.producer.resolve()
    for directory in (
        "lib/src",
        "test/experiments/render",
        "test/experiments/transport",
        "test/experiments/buildings",
    ):
        sys.path.insert(0, str(root / directory))
    import numpy as np
    from experiment_output import create_output
    from field_view_cost import measured
    from road_normal_study import generate, read_row

    files = [
        root / "test/experiments/transport" / name
        for name in (
            "road_normal_study.py",
            "road_normal_lod.py",
            "road_normal_cone.py",
            "road_normal_math.py",
            "road_normal_hulls.py",
            "road_surface_lod.py",
            "road_surface_model.py",
            "road_surface_frames.py",
        )
    ]
    files.append(root / "test/experiments/render/field_view_cost.py")
    if subprocess.check_output(
        ["git", "-C", str(root), "status", "--porcelain", "--", *map(str, files)],
        text=True,
    ):
        raise ValueError("Freeze all measured producer sources first")
    driver = Path(__file__).resolve()
    if subprocess.check_output(
        [
            "git",
            "-C",
            str(driver.parents[3]),
            "status",
            "--porcelain",
            "--",
            str(driver),
        ],
        text=True,
    ):
        raise ValueError("Freeze the cost driver first")
    row, manifest = read_row(args.ordinal)
    reference_report = json.loads((args.reference / "report.json").read_text())
    if (
        reference_report["ordinal"] != args.ordinal
        or reference_report["method"] != args.method
        or reference_report["archive_sha256"] != manifest["archive_sha256"]
        or not reference_report["accepted"]
    ):
        raise ValueError("Same accepted complete original and method required")
    keys = (
        "stations_m",
        "lateral_m",
        "positions_enh_m",
        "normals_enh",
        "station_lateral_m",
        "indices",
        "cell_vertex_rows",
    )
    with np.load(args.reference / "plan.npz", allow_pickle=False) as saved:
        expected = {key: saved[key] for key in keys}
    create_output(args.output)
    load_before = list(os.getloadavg())

    def call():
        return generate(
            row,
            args.method,
            reference_report["position_limit_m"],
            np.deg2rad(reference_report["normal_limit_degrees"]),
            reference_report["max_queries"],
            reference_report["max_vertices"],
            **(
                {"first_valid_split": True}
                if reference_report.get("first_valid_split", False)
                else {}
            ),
        )[0]

    plan, costs = measured(call, args.output, args.method)
    for key in keys:
        actual, before = plan[key], expected[key]
        if (
            actual.dtype != before.dtype
            or actual.shape != before.shape
            or actual.tobytes() != before.tobytes()
        ):
            raise ValueError("Complete stored plan words differ: " + key)
    report = {
        "producer_commit": subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
        ).strip(),
        "driver_commit": subprocess.check_output(
            ["git", "-C", str(driver.parents[3]), "rev-parse", "HEAD"], text=True
        ).strip(),
        "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(),
        "producer_source_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in files
        },
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load_before,
        "load_after": list(os.getloadavg()),
        "ordinal": args.ordinal,
        "method": args.method,
        "position_limit_m": reference_report["position_limit_m"],
        "normal_limit_degrees": reference_report["normal_limit_degrees"],
        "archive_sha256": manifest["archive_sha256"],
        "reference_report_sha256": hashlib.sha256(
            (args.reference / "report.json").read_bytes()
        ).hexdigest(),
        "reference_plan_sha256": hashlib.sha256(
            (args.reference / "plan.npz").read_bytes()
        ).hexdigest(),
        "same_complete_plan_words": True,
        "array_sha256": {
            key: hashlib.sha256(plan[key].tobytes()).hexdigest() for key in keys
        },
        "held_plan_array_bytes": sum(plan[key].nbytes for key in keys),
        "known_array_copy_bytes_lower_bound": sum(
            plan[key].nbytes
            for key in ("positions_enh_m", "normals_enh", "station_lateral_m")
        )
        + len(plan["stations_m"]) * len(plan["lateral_m"]) * 3 * 4,
        "normal_arithmetic_work": plan.get("normal_arithmetic_work"),
        "whole_normal_queries": plan["whole_normal_queries"],
        "source_pose_evaluations": plan["source_pose_evaluations"],
        "vertices": len(plan["positions_enh_m"]),
        "triangles": len(plan["indices"]),
        "costs": costs,
        "scope": "Seven ordinary generation rounds plus one separate traced round from the same decoded raw recipe. Includes Curve reconstruction, interval qualification, refinement and final stored arrays. Input decode, diagnostics, export, GC and output-word verification excluded from timers. Memray includes Python/native allocation requests and scoped peak, not whole-process/UMA/device memory. Known copies cover final position/normal/chart concatenations and global Float32 position conversion only; total copies are unmeasured. No native/PBR/World/physical-profile/p99 admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "producer": report["producer_commit"],
                "method": args.method,
                "costs": costs,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--ordinal", type=int, required=True)
    parser.add_argument("--method", choices=("adaptive", "uniform"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
