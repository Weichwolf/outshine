"""Reproduce complete two-flight bodies and compare qualified construction costs."""

import argparse
import gc
import json
import os
import platform
import resource
import subprocess
import sys
import tracemalloc
from pathlib import Path

import numpy as np
import scipy
import shapely
from mvt_independent_flight_body import construct
from mvt_native_body_display import display


def summary(case):
    plan, data, products = case["solver_plan"], case["data"], case["products"]
    return {
        "sources": len(plan["rows"]),
        "flights": len(plan["stairs"]),
        "fixed_profiles": case["fixed_profiles"],
        "float32_orientation": case["float32_orientation"],
        "float32_body_volumes_m3": case["float32_body_volumes_m3"],
        "global_vertical_clearance_bound_m": case["global_vertical_clearance_bound_m"],
        "geometry": case["geometry"],
        "height_vertices": len(plan["xy"]),
        "height_owned_face_references": sum(map(len, plan["parts"])),
        "arrangement": plan["note"],
        "station_refinement": plan["station_refinement"],
        "navigation_nodes": len(products["navigation"]["nodes_xyz_m"]),
        "terrain_reservations": len(products["terrain_reservations"]),
        "solve": case["solve"],
        "profile_audit": case["proof"],
        "export_array_count": len(data),
        "selected_export_array_bytes": sum(a.nbytes for a in data.values()),
        "float32_audit_position_cast_bytes": len(data["body_vertices"]) * 12,
        "allocation_scope": (
            "Station refinement counts explicit XY vertex copies and bytes; "
            "export bytes and audit position cast are selected arrays, not total "
            "copies or unique UMA residency. Cumulative allocation count is unmeasured."
        ),
    }


def measure(options, repeats):
    timings = []
    for _ in range(repeats + 1):
        case = construct(**options)
        timings.append(case["seconds"])
        del case
        gc.collect()
    tracemalloc.start()
    case = construct(**options)
    _current, peak = tracemalloc.get_traced_memory()
    live_blocks = sum(
        s.count for s in tracemalloc.take_snapshot().statistics("filename")
    )
    tracemalloc.stop()
    report = summary(case)
    report.update(
        {
            "first_seconds": timings[0],
            "subsequent_seconds": timings[1:],
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": live_blocks,
            "memory_scope": (
                "Separate subsequent complete construction, audits and products; "
                "prior cases released. Native library allocations may escape tracing; "
                "live blocks are not cumulative allocations. Plotting/export excluded."
            ),
        }
    )
    return case, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--arrangement", choices=("stacked", "crossed"), default="stacked"
    )
    parser.add_argument(
        "--solver", choices=("highs-ds", "highs-ipm"), default="highs-ds"
    )
    parser.add_argument(
        "--cut-scope",
        choices=("global", "flight_footprint"),
        default="flight_footprint",
    )
    parser.add_argument("--rigid", action="store_true")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("bounded subsequent measurements required: 1..21")
    args.output.mkdir(parents=True, exist_ok=True)
    options = {
        "arrangement": args.arrangement,
        "solver": args.solver,
        "cut_scope": args.cut_scope,
        "global_pose": (0.437, (37, -81)) if args.rigid else None,
    }
    report = {
        "scope": (
            "Controlled composition of retained inputs; complete Python bodies, "
            "local height-envelope Nav and terrain hints. No native/device/World "
            "or full swept walking-route admission."
        ),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "load_average_at_start": os.getloadavg(),
        "library_versions": {
            "numpy": np.__version__,
            "scipy": scipy.__version__,
            "shapely": shapely.__version__,
        },
        "options": options,
    }
    try:
        case, measured = measure(options, args.repeats)
    except ValueError as error:
        report.update(status="rejected", reason=str(error))
        (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        raise
    report.update(status="qualified_local_control", **measured)
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report["process_peak_rss_bytes"] = rss if sys.platform == "darwin" else rss * 1024
    np.savez(args.output / "body.npz", **case["data"])
    (args.output / "shared-plan.json").write_text(
        json.dumps(case["products"], indent=2) + "\n"
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.render:
        display(case["plan"], case["data"], args.output)
    print(
        json.dumps(
            {
                "status": report["status"],
                "options": options,
                "geometry": report["geometry"],
            }
        )
    )


if __name__ == "__main__":
    main()
