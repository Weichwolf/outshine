"""Replay explicitly analytical bridge sources with bound DEM and finite work reports."""

import argparse
import json
import time
from contextlib import nullcontext
from pathlib import Path

import numpy as np
from canonical_bridge_cases import CASES
from outshine_ref.transport.canonical_ports import derive_canonical_ports
from replay_canonical_ports import process_peak, serializable


def allocations(path):
    import memray

    reader = memray.FileReader(str(path))
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.MUNMAP,
        memray.AllocatorType.PYMALLOC_FREE,
    }
    requests, requested = 0, 0
    for record in reader.get_allocation_records():
        if record.allocator not in frees:
            requests += record.n_allocations
            requested += record.size
    return {
        "allocation_requests": requests,
        "cumulative_requested_bytes": requested,
        "tracked_heap_high_watermark_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "scope": "derive only, including cold solver imports; traced Python allocator; allocation bytes are not copied bytes",
    }


def plot_report(source, demands, result, terrain, target, case):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    geographic = np.array([node.longitude_latitude for node in source.nodes])
    axis = int(np.argmax(np.ptp(geographic, axis=0)))
    domain = np.linspace(geographic[0], geographic[-1], 1001)
    cuts = np.array([d.anchor_longitude_latitude for d in demands])
    valid = np.array([v is not None for v in result.values])
    height = np.array([v.height_m if v else np.nan for v in result.values])
    figure, panels = plt.subplots(1, 2, figsize=(12, 4.5), layout="constrained")
    panels[0].plot(
        domain[:, axis], terrain(domain), label="bound DEM (diagnostic samples)"
    )
    panels[0].scatter(
        cuts[valid, axis],
        height[valid],
        marker="+",
        color="darkorange",
        label="numeric canonical ports",
    )
    if not valid.all():
        panels[0].scatter(
            cuts[~valid, axis],
            terrain(cuts[~valid]),
            color="crimson",
            marker="x",
            label="Pending: DEM input, no output height",
        )
    panels[0].set(ylabel="explicit model geodetic height (m)")
    panels[1].plot(
        cuts[:, axis],
        [v.longitudinal_grade if v else np.nan for v in result.values],
        "o",
        label="longitudinal grade",
    )
    panels[1].plot(
        cuts[:, axis],
        [-np.tan(v.crossfall_rad) if v else np.nan for v in result.values],
        "+",
        label="left cross grade",
    )
    panels[1].set(ylabel="rise / run in own port ENU")
    for panel in panels:
        panel.set(xlabel=("longitude" if axis == 0 else "latitude") + " (deg)")
        panel.legend()
    figure.suptitle(
        case
        + ": analytical topology; source-bound height recipe, no native bridge/clearance admission"
    )
    target.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(target, dpi=140)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=CASES, required=True)
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--bridge-second-derivative-per-m", type=float, required=True)
    parser.add_argument("--maximum-prior-change-m", type=float, required=True)
    parser.add_argument("--allocation-trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plot", type=Path)
    args = parser.parse_args()
    for path in (args.output, args.plot, args.allocation_trace):
        if path and path.exists():
            parser.error("fresh explicit outputs required")
    source, demands, terrain, binding = CASES[args.case]()
    if args.reverse:
        demands = demands[::-1]
    tracking = nullcontext()
    if args.allocation_trace:
        import memray

        args.allocation_trace.parent.mkdir(parents=True, exist_ok=True)
        tracking = memray.Tracker(
            str(args.allocation_trace), trace_python_allocators=True
        )
    wall, cpu = time.perf_counter(), time.process_time()
    with tracking:
        result = derive_canonical_ports(
            source,
            demands,
            terrain,
            terrain_binding=binding,
            maximum_prior_change_m=args.maximum_prior_change_m,
            bridge_second_derivative_per_m=args.bridge_second_derivative_per_m,
        )
    wall, cpu = time.perf_counter() - wall, time.process_time() - cpu
    peak, peak_source = process_peak()
    measurement = {
        "wall_ms": wall * 1000,
        "process_cpu_ms": cpu * 1000,
        "process_peak_rss_bytes": peak,
        "rss_source": peak_source,
        "allocation_tracing": bool(args.allocation_trace),
        "scope": "Offline Python, whole-process RSS including loaded inputs; no native or device proof",
    }
    if args.allocation_trace:
        measurement["allocations"] = allocations(args.allocation_trace)
    report = {
        "schema": "outshine/canonical-bridge-replay/1",
        "case": args.case,
        "demands": demands,
        "values": result.values,
        "reasons": result.reasons,
        "report": result.report,
        "measurement": measurement,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        json.dump(serializable(report), stream, indent=2, allow_nan=False)
        stream.write("\n")
    if args.plot:
        plot_report(source, demands, result, terrain, args.plot, args.case)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "ports": len(demands),
                "numeric": sum(v is not None for v in result.values),
                **measurement,
            }
        )
    )
    return 0 if all(result.values) else 1


if __name__ == "__main__":
    raise SystemExit(main())
