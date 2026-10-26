"""Actual cap/riser chains for the six existing local navigation edges."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from compare_stair_envelopes import run
from mvt_native_body_display import display
from outshine_ref.contracts import Mesh
from stair_route_audit import audit_section
from stair_route_section import trace_section


def _route_note(edge, section, mesh, top_count, nav):
    source = edge["source_id"]
    profile = np.array(nav["axis_profiles_xyz_m"][source])
    axis = shapely.LineString(np.array(nav["axes_xyz_m"][source])[:, :2])
    reference = shapely.line_locate_point(axis, shapely.points(profile[:, :2]))
    stations = shapely.line_locate_point(axis, shapely.points(section["xyz_m"][:, :2]))
    envelope = np.interp(stations, reference, profile[:, 2])
    return {
        "source_id": source,
        "nodes": edge["nodes"],
        "xyz_m": section["xyz_m"].tolist(),
        "axis_parameter": section["axis_parameter"].tolist(),
        "arclength_m": section["arclength_m"].tolist(),
        "segment_face_ids": section["segment_face_ids"],
        "riser_segments": [
            any(i >= top_count for i in ids) for ids in section["segment_face_ids"]
        ],
        "reference_envelope_z_m": envelope.tolist(),
        "maximum_envelope_height_difference_m": float(
            abs(section["xyz_m"][:, 2] - envelope).max()
        ),
        "audit": audit_section(mesh, section),
        "work": section["work"],
        "scope": section["scope"],
    }


def construct_routes(method, precision, trace):
    plan, data, products, body_report = run("envelope_indexed_band_remesh", False)
    nav = products["navigation"]
    vertices, nodes = data["body_vertices"], np.array(nav["nodes_xyz_m"])
    if precision == "float32":
        vertices, nodes = (
            vertices.astype("f4").astype(float),
            nodes.astype("f4").astype(float),
        )
    risers = np.flatnonzero(data["wall_riser"] == 1)
    mesh = Mesh(vertices, np.vstack([data["top_faces"], data["wall_faces"][risers]]))
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    sections = [
        trace_section(mesh, nodes[edge["nodes"], :2], method=method)
        for edge in nav["edges"]
    ]
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    routes = [
        _route_note(edge, section, mesh, len(data["top_faces"]), nav)
        for edge, section in zip(nav["edges"], sections, strict=True)
    ]
    report = {
        "method": method,
        "precision": precision,
        "instrumented": trace,
        "route_build_s": elapsed,
        "route_mesh_array_bytes": mesh.vertices.nbytes + mesh.triangles.nbytes,
        "top_face_count": len(data["top_faces"]),
        "riser_wall_indices": risers.tolist(),
        "work": {
            key: sum(section["work"][key] for section in sections)
            for key in sections[0]["work"]
        },
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "body_report": body_report,
        "scope": "Build-only time/tracing for six exact route sections after the same body/graph preparation, before independent 3D audits, envelope comparison, images and exports. Mesh ownership/casting precede measurement; RSS is whole-process peak. No walking-body/headroom/outside route or native/device/frame admission.",
    }
    return plan, data, mesh, routes, report


def _display_route(routes, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    route = max(routes, key=lambda r: np.sum(r["riser_segments"]))
    xyz = np.array(route["xyz_m"])
    distance = np.asarray(route["axis_parameter"]) * np.linalg.norm(
        xyz[-1, :2] - xyz[0, :2]
    )
    figure, axis = plt.subplots(figsize=(10, 4))
    axis.plot(distance, xyz[:, 2], color="#38634a", label="actual cap/riser section")
    axis.plot(
        distance,
        route["reference_envelope_z_m"],
        "--",
        color="#bf7138",
        label="existing height envelope",
    )
    axis.set(
        xlabel="projected axis distance / m",
        ylabel="height / m",
        title="Actual indexed stair support along the unchanged local graph edge",
    )
    axis.legend()
    figure.tight_layout()
    figure.savefig(output / "stair-route.png", dpi=140)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("bounds", "scan"), default="bounds")
    parser.add_argument(
        "--precision", choices=("float32", "float64"), default="float32"
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    plan, data, mesh, routes, report = construct_routes(
        args.method, args.precision, args.trace_memory
    )
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(
        args.output / "route-mesh.npz", vertices=mesh.vertices, faces=mesh.triangles
    )
    (args.output / "routes.json").write_text(json.dumps(routes, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(plan, data, args.output)
        _display_route(routes, args.output)
    print(
        json.dumps(
            {
                "method": args.method,
                "route_build_s": report["route_build_s"],
                "routes": len(routes),
            }
        )
    )


if __name__ == "__main__":
    main()
