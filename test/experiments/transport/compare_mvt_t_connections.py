"""Compare identical procedural proposals, query work and fresh-worker memory."""

import argparse
import copy
import json
import os
import resource
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

from mvt_t_connection_inputs import inputs, vienna_inputs
from mvt_t_connections import plan_connections


def stable(plan):
    return {
        k: v
        for k, v in plan.items()
        if k not in ("work", "input_prepare_s", "index_build_s", "elapsed_s")
    }


def selected_rows(name):
    if name == "vienna":
        return vienna_inputs()
    _case, original = inputs()
    if name == "original":
        return original
    count = int(name.removeprefix("copies-"))
    rows = []
    for i in range(count):
        for source in copy.deepcopy(original):
            source["source_id"] = f"copy:{i}:" + source["source_id"]
            source["xy_m"] = [
                [x + 300 * (i % 16), y + 300 * (i // 16)] for x, y in source["xy_m"]
            ]
            rows.append(source)
    return rows


def worker(name, method):
    rows = selected_rows(name)
    # Time and memory runs are separate: tracemalloc changes Python/scan costs.
    began = time.perf_counter()
    plan = plan_connections(rows, method=method, max_candidates=2000000)
    elapsed = time.perf_counter() - began
    tracemalloc.start()
    measured = plan_connections(rows, method=method, max_candidates=2000000)
    _current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if stable(plan) != stable(measured):
        raise ValueError("memory instrumentation changed the proposal")
    coordinate_bytes = sum(len(r["xy_m"]) for r in rows) * 2 * 8
    output_coordinate_bytes = sum(len(p) for p in plan["axes_xy_m"].values()) * 2 * 8
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return {
        "plan": plan,
        "elapsed_untraced_s": elapsed,
        "python_traced_peak_bytes": peak,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        "numeric_input_coordinate_bytes": coordinate_bytes,
        "numeric_output_coordinate_bytes": output_coordinate_bytes,
        "input_json_bytes": len(json.dumps(rows).encode()),
        "output_json_bytes": len(json.dumps(plan).encode()),
        "rss_scope": "whole fresh worker including imports, both runs and GEOS; not app/device RAM",
        "bytes_scope": "Float64 coordinate payload and serialized JSON sizes; not measured IO or native copies",
    }


def plot(original, plan, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from shapely import LineString

    fig, axes = plt.subplots(1, 2, figsize=(12, 6))
    upper = next(c for c in plan["connections"] if c["advance_m"] > 0)
    for panel in axes:
        for source in original:
            points = source["xy_m"]
            constructed = plan["axes_xy_m"][source["source_id"]]
            color = "#bb6537" if source["kind"] == "steps" else "#37718e"
            shape = LineString(points).buffer(source["half_width_m"], cap_style="flat")
            x, y = shape.exterior.xy
            panel.fill(x, y, color=color, alpha=0.13)
            panel.plot(
                *zip(*constructed), color=color, lw=1.5, label=source["source_id"]
            )
            panel.plot(*zip(*points), "o", color=color, ms=3)
        for contact in plan["connections"]:
            panel.plot(*contact["xy_m"], marker="x", color="black", ms=9)
        panel.set_aspect("equal")
        panel.set_xlabel("local east / m")
        panel.set_ylabel("local north / m")
        panel.grid(alpha=0.2)
    axes[0].set_title("Three original parts; two shared construction contacts")
    axes[0].legend(fontsize=8)
    x, y = upper["source_original_xy_m"]
    axes[1].set_xlim(x - 1.1, x + 1.1)
    axes[1].set_ylim(y - 1.1, y + 1.1)
    axes[1].annotate(
        "original stair end",
        (x, y),
        (x + 0.15, y - 0.65),
        arrowprops={"arrowstyle": "->"},
        fontsize=9,
    )
    axes[1].annotate(
        "shared derived node",
        upper["xy_m"],
        (x - 1, y + 0.75),
        arrowprops={"arrowstyle": "->"},
        fontsize=9,
    )
    axes[1].set_title(f"Forward extension {upper['advance_m']:.6f} m")
    fig.suptitle("2D recipe proposal only: height/terrain/native admission pending")
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)


def compare(output):
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "cases": {},
        "pending": [
            "whole 2.5D profiles and landing cuts",
            "endpoint-role/provider-error qualification",
            "native consumer and device admission",
        ],
    }
    for name in ("original", "vienna", "copies-32", "copies-128"):
        results = {}
        for method in ("index", "scan"):
            run = subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--worker",
                    name,
                    "--method",
                    method,
                ],
                text=True,
                capture_output=True,
                check=True,
                env=os.environ.copy(),
            )
            results[method] = json.loads(run.stdout)
        if stable(results["index"]["plan"]) != stable(results["scan"]["plan"]):
            raise ValueError(f"indexed/reference proposals differ for {name}")
        report["cases"][name] = results
        print(
            json.dumps(
                {
                    "case": name,
                    "connections": len(results["index"]["plan"]["connections"]),
                    "tests": {
                        m: r["plan"]["work"]["candidate_tests"]
                        for m, r in results.items()
                    },
                    "elapsed_s": {
                        m: r["elapsed_untraced_s"] for m, r in results.items()
                    },
                }
            ),
            flush=True,
        )
    _case, original = inputs()
    report["reach_sensitivity"] = {
        str(reach): len(plan_connections(original, reach_m=reach)["connections"])
        for reach in (0, 0.1, 0.28, 0.3, 0.5, 0.75)
    }
    plot(
        original,
        report["cases"]["original"]["index"]["plan"],
        output / "connections.png",
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--worker", choices=("original", "vienna", "copies-32", "copies-128")
    )
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker, args.method)))
    elif args.output is not None:
        compare(args.output)
    else:
        parser.error("--output or --worker required")


if __name__ == "__main__":
    main()
