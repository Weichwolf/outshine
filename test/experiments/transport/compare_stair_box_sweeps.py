"""Same explicit box, unchanged local curves and full body: scan or outward bounds."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_stair_route_sections import construct_routes
from outshine_ref.contracts import Mesh
from stair_box_sweep import sweep_surfaces
from stair_box_witness import penetration_witness


def run(method, precision, lower, upper, trace):
    _plan, data, support, routes, source = construct_routes("bounds", precision, False)
    mesh = Mesh(support.vertices, data["body_faces"])
    paths = [route["xyz_m"] for route in routes]
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    events, work = sweep_surfaces(mesh, paths, lower, upper, method=method)
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    rows = []
    for route_id, route in enumerate(routes):
        selected = [event[1:] for event in events if event[0] == route_id]
        hits = [event for event in selected if event[2] == "interior"]
        witnesses = []
        for segment, face, _kind in hits[:: max(1, len(hits) // 8)][:8]:
            points = np.array(route["xyz_m"])
            witness = penetration_witness(
                mesh.vertices[mesh.triangles[face]],
                *points[segment : segment + 2],
                lower,
                upper,
            )
            witnesses.append(dict(segment=segment, face=face, **witness))
        rows.append(
            dict(route, surface_events=selected, independent_witnesses=witnesses)
        )
    report = {
        "method": method,
        "precision": precision,
        "instrumented": trace,
        "box_lower_m": list(lower),
        "box_upper_m": list(upper),
        "sweep_query_s": elapsed,
        "work": work,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "source_route_report": source,
        "scope": "Complete six-route surface sweep after the same body/routes/owned full Mesh. Includes outward bounds, lazy exact vertex/triangle preparation and exact SAT/events. Independent LP witnesses, image/export follow measurement; RSS is whole-process peak. Explicit fixed-axis rigid box is a test hypothesis, not a character norm. Surface hits do not decide solid containment, gait, floor selection, outside-route admission or device/frame cost. No GEOS/OSQP/HiGHS hot-path use.",
    }
    return mesh, rows, report


def display(mesh, rows, lower, upper, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle

    route = next(route for route in rows if any(route["riser_segments"]))
    chosen = max(
        route["independent_witnesses"], key=lambda w: w["reconstructed_box_margin_m"]
    )
    points = np.array(route["xyz_m"])
    direction = points[-1, :2] - points[0, :2]
    direction /= np.linalg.norm(direction)
    station = (points[:, :2] - points[0, :2]) @ direction
    foot, point = np.array(chosen["foot_xyz_m"]), np.array(chosen["point_xyz_m"])
    center = float((foot[:2] - points[0, :2]) @ direction)
    corners = (
        np.array([[x, y] for x in (lower[0], upper[0]) for y in (lower[1], upper[1])])
        @ direction
    )
    figure, axes = plt.subplots(1, 2, figsize=(12, 6))
    axes[0].plot(
        station, points[:, 2], color="#375a42", label="unchanged actual support curve"
    )
    axes[0].add_patch(
        Rectangle(
            (center + corners.min(), foot[2] + lower[2]),
            np.ptp(corners),
            upper[2] - lower[2],
            facecolor="#ba4d40",
            alpha=0.2,
            edgecolor="#963b35",
        )
    )
    witness_station = float((point[:2] - points[0, :2]) @ direction)
    axes[0].plot(
        witness_station,
        point[2],
        "x",
        color="#963b35",
        markersize=10,
        label="exact reconstructed interior witness",
    )
    triangle = mesh.vertices[mesh.triangles[chosen["face"]]]
    triangle_station = (triangle[:, :2] - points[0, :2]) @ direction
    axes[0].plot(
        np.r_[triangle_station, triangle_station[0]],
        np.r_[triangle[:, 2], triangle[0, 2]],
        color="#bb793a",
        linewidth=2,
        label="hit original body triangle",
    )
    axes[0].set(
        xlim=(center - 0.65, center + 0.65),
        ylim=(foot[2] - 0.2, foot[2] + upper[2] + 0.1),
        xlabel="axis projection / m",
        ylabel="height / m",
        title="Local stair: supported point does not admit the rigid box",
    )
    axes[0].legend(fontsize=8, loc="upper left")
    for x in (-1, 1):
        axes[1].add_patch(
            Rectangle((x - 0.125, 0), 0.25, 1, fill=False, edgecolor="#375a42")
        )
    axes[1].add_patch(
        Rectangle(
            (-1.125, 0),
            2.25,
            1,
            facecolor="#cebe9b",
            alpha=0.25,
            label="continuous translated box sweep",
        )
    )
    axes[1].plot(
        [0, 0], [0, 2], color="#963b35", linewidth=3, label="thin intervening wall"
    )
    axes[1].add_patch(Rectangle((-0.125, 0), 0.25, 1, fill=False, edgecolor="#963b35"))
    axes[1].set(
        xlim=(-1.4, 1.4),
        ylim=(-0.1, 2.1),
        xlabel="x / m",
        ylabel="height / m",
        title="Both endpoint poses fit; the intervening sweep hits",
    )
    axes[1].legend(fontsize=8)
    figure.suptitle(
        "Explicit fixed-axis test boxes; no locomotion or outside-domain admission"
    )
    figure.tight_layout()
    figure.savefig(output / "box-sweep.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("scan", "bounds"), default="bounds")
    parser.add_argument(
        "--precision", choices=("float32", "float64"), default="float32"
    )
    parser.add_argument("--box-lower", required=True, nargs=3, type=float)
    parser.add_argument("--box-upper", required=True, nargs=3, type=float)
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    mesh, rows, report = run(
        args.method, args.precision, args.box_lower, args.box_upper, args.trace_memory
    )
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(
        args.output / "full-body.npz", vertices=mesh.vertices, faces=mesh.triangles
    )
    (args.output / "routes.json").write_text(json.dumps(rows, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(
            mesh, rows, np.array(args.box_lower), np.array(args.box_upper), args.output
        )
    print(
        json.dumps(
            {
                "method": args.method,
                "sweep_query_s": report["sweep_query_s"],
                "work": report["work"],
            }
        )
    )


if __name__ == "__main__":
    main()
