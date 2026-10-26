"""Continuous projected side room: share one layer projection or rebuild per route."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from compare_stair_route_sections import construct_routes
from stair_route_width import measure_routes, projected_support, side_clearance


def run(method, precision, trace):
    _plan, _data, mesh, routes, source = construct_routes("bounds", precision, False)
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    clearances, work = measure_routes(mesh, routes, method=method)
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    rows = [
        dict(route, projected_side_room=clearance)
        for route, clearance in zip(routes, clearances, strict=True)
    ]
    report = {
        "method": method,
        "precision": precision,
        "instrumented": trace,
        "clearance_build_s": elapsed,
        "work": work,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "source_route_report": source,
        "scope": "Projection plus all six complete-route covers/distance/witness queries, after identical body and actual-route construction. Images and serialization follow measurement. Python tracing omits part of GEOS/native allocations; RSS is whole-process peak. No global body norm, headroom, alternative-route optimum, outside or device/frame admission.",
    }
    return mesh, rows, report


def _plot_shape(axis, domain):
    for polygon in shapely.get_parts(domain):
        xy = np.array(polygon.exterior.coords)
        axis.fill(xy[:, 0], xy[:, 1], color="#d5e1d3", edgecolor="#6a7b67")
        for ring in polygon.interiors:
            xy = np.array(ring.coords)
            axis.fill(xy[:, 0], xy[:, 1], color="white")


def _display(mesh, routes, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Circle
    from test_stair_route_width import neck

    figure, axes = plt.subplots(1, 2, figsize=(12, 6))
    domain, _work = projected_support(mesh)
    _plot_shape(axes[0], domain)
    for route in routes:
        xy = np.array(route["xyz_m"])[:, :2]
        axes[0].plot(xy[:, 0], xy[:, 1], color="#375a42")
        width = route["projected_side_room"]
        witness = np.array(width["witness_xy_m"])
        radius = width["available_radius_m"]
        if radius and radius > 0:
            axes[0].add_patch(Circle(witness[0], radius, fill=False, color="#c57530"))
            axes[0].plot(witness[:, 0], witness[:, 1], color="#c57530")
            axes[0].annotate(
                f"r = {radius:.3f} m",
                witness[0],
                xytext=(6, 6),
                textcoords="offset points",
            )
        else:
            axes[0].plot(*witness[0], "x", color="#a73a39")
    axes[0].set(
        title="Selected local support layer: side room and four finite cuts",
        xlabel="x / m",
        ylabel="y / m",
    )
    counter = neck()
    _plot_shape(axes[1], counter)
    xy = np.array([[-1.5, 0], [1.5, 0]])
    axes[1].plot(xy[:, 0], xy[:, 1], "o-", color="#a73a39")
    for center in xy:
        axes[1].add_patch(Circle(center, 0.14, fill=False, color="#375a42"))
    width = side_clearance(counter, xy)
    witness = np.array(width["witness_xy_m"])
    axes[1].plot(witness[:, 0], witness[:, 1], color="#c57530", linewidth=3)
    axes[1].set(
        title="Both support disks fit; the connecting neck has only r = 0.10 m",
        xlabel="x / m",
        ylabel="y / m",
    )
    for axis in axes:
        axis.set_aspect("equal")
    figure.tight_layout()
    figure.savefig(output / "projected-side-room.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("shared", "rebuild"), default="shared")
    parser.add_argument(
        "--precision", choices=("float32", "float64"), default="float32"
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    mesh, routes, report = run(args.method, args.precision, args.trace_memory)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(
        args.output / "route-mesh.npz", vertices=mesh.vertices, faces=mesh.triangles
    )
    (args.output / "routes.json").write_text(json.dumps(routes, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        _display(mesh, routes, args.output)
    print(
        json.dumps(
            {"method": args.method, "clearance_build_s": report["clearance_build_s"]}
        )
    )


if __name__ == "__main__":
    main()
