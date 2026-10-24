"""Export the fixed World stair reference, exact bindings and scoped body costs."""

import argparse
import gc
import hashlib
import json
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_mvt_native_body import orientation_counts
from mvt_native_body_display import display
from outshine_ref.contracts import Mesh
from surface_solid_checks import audit_closed
from world_stair_reference import FIXTURE, construct


def display_capture(capture, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    raw = np.asarray(capture["reference_positions_east_up_south_m"]).reshape(-1, 3)
    points = raw[:, [0, 2, 1]].copy()
    points[:, 1] *= -1
    faces = np.asarray(capture["reference_triangles"]).reshape(-1, 3)
    figure = plt.figure(figsize=(8, 7))
    axes = figure.add_subplot(projection="3d")
    axes.add_collection3d(
        Poly3DCollection(
            points[faces],
            facecolors=(0.57, 0.72, 0.53),
            edgecolors=(0, 0, 0, 0.2),
            linewidths=0.5,
        )
    )
    for ids in capture["reference_boundary_vertex_indices"]:
        xyz = points[ids]
        axes.plot(xyz[:, 0], xyz[:, 1], xyz[:, 2], "k.-")
    low, high = points.min(axis=0), points.max(axis=0)
    axes.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        zlim=(low[2], high[2]),
        xlabel="east / m",
        ylabel="north / m",
        zlabel="height / m",
        title="Captured actual World ramp input / fixed exterior profiles",
    )
    axes.set_box_aspect(high - low)
    axes.view_init(elev=32, azim=-60)
    figure.tight_layout()
    figure.savefig(output / "captured-ramp.png", dpi=150, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error("bounded positive repeat count required")
    args.output.mkdir(parents=True, exist_ok=True)
    capture = json.loads(FIXTURE.read_text())
    start_load, timings = os.getloadavg(), []
    for _ in range(args.repeats + 1):
        began = time.perf_counter()
        case = construct(capture)
        timings.append(time.perf_counter() - began)
        del case
        gc.collect()
    tracemalloc.start()
    case = construct(capture)
    _current, peak = tracemalloc.get_traced_memory()
    live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    data = case["data"]
    mesh = Mesh(
        data["body_vertices"].astype(np.float32).astype(float), data["body_faces"]
    )
    orientation = orientation_counts(data)
    volume = audit_closed(mesh)
    if orientation["invalid_float32_faces"] or volume <= 0:
        raise ValueError("actual Float32 body failed validation")
    report = {
        "fixed_input_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "native_input_provenance": capture["provenance"],
        "first_s": timings[0],
        "following_s": timings[1:],
        "median_s": float(np.median(timings[1:])),
        "start_load": start_load,
        "python_traced_peak_bytes": peak,
        "python_traced_live_blocks": live,
        "process_peak_rss_bytes_before_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "geometry": case["geometry"],
        "recipe": case["recipe"],
        "float32_orientation": orientation,
        "float32_closed_volume_m3": volume,
        "body_array_bytes": sum(a.nbytes for a in data.values()),
        "input_xy_height_bytes": case["plan"]["xy"].nbytes + case["heights"].nbytes,
        "input_face_index_bytes": sum(a.nbytes for a in case["plan"]["parts"]),
        "scope": "Existing Python body construction and exact bindings; separate tracing, RSS before export/plot. Libraries already imported. No native time or device/World admission; selected bytes are not total copies or cumulative allocations.",
    }
    np.savez(args.output / "body.npz", **data)
    np.savez(
        args.output / "body-input.npz",
        xy=case["plan"]["xy"],
        heights=case["heights"],
        **{f"source_faces_{i}": faces for i, faces in enumerate(case["plan"]["parts"])},
    )
    bindings = {
        "captured_source": capture,
        "source_instances_in_mask_bit_order": case["plan"]["rows"],
        "ports": case["ports"],
        "scope": "Body-kernel reference input and explicit part/port bindings; not a shared solver plan, Nav route, terrain-role inference or new product format.",
    }
    for name, value in (("report", report), ("source-bindings", bindings)):
        (args.output / (name + ".json")).write_text(json.dumps(value, indent=2) + "\n")
    if args.render:
        display_capture(capture, args.output)
        display(case["plan"], data, args.output)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
