"""Compare exact scalar/batch body supports on the existing fixed World source."""

import argparse
import gc
import json
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from body_support_spans import support_points, trace
from world_stair_reference import FIXTURE, construct


def compare(body, path, repeats):
    timings = {"scalar": [], "batch": []}
    for i in range(repeats + 1):
        order = ("scalar", "batch") if i % 2 == 0 else ("batch", "scalar")
        for strategy in order:
            began = time.perf_counter()
            cpu = time.process_time()
            result = trace(body, path, 7, strategy=strategy)
            cpu_elapsed = time.process_time() - cpu
            elapsed = time.perf_counter() - began
            timings[strategy].append((elapsed, cpu_elapsed))
            del result
            gc.collect()
    results, reports = {}, {}
    for strategy, samples in timings.items():
        gc.collect()
        tracemalloc.start()
        result = trace(body, path, 7, strategy=strategy)
        _current, peak = tracemalloc.get_traced_memory()
        live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
        tracemalloc.stop()
        wall, cpu = np.asarray(samples).T
        results[strategy] = result
        reports[strategy] = {
            "first_s": float(wall[0]),
            "following_s": wall[1:].tolist(),
            "median_s": float(np.median(wall[1:])),
            "first_cpu_s": float(cpu[0]),
            "following_cpu_s": cpu[1:].tolist(),
            "median_cpu_s": float(np.median(cpu[1:])),
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": live,
            "work": result["work"],
        }
    overhead = []
    for _ in range(101):
        began, cpu = time.perf_counter(), time.process_time()
        cpu_elapsed = time.process_time() - cpu
        overhead.append((time.perf_counter() - began, cpu_elapsed))
    return (
        results,
        reports,
        {
            "order": "paired, alternated every repetition",
            "observer_median_wall_cpu_s": np.median(overhead, axis=0).tolist(),
            "observer_scope": "Empty wall/CPU timer pairs; raw trace times are not corrected.",
        },
    )


def display(body, support, original, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    endpoints = support_points(support)
    vertices = body["body_vertices"].astype(float)
    figure = plt.figure(figsize=(14, 7))
    axes = figure.add_subplot(1, 2, 1, projection="3d")
    faces = np.vstack([body["top_faces"], body["wall_faces"]])
    axes.add_collection3d(
        Poly3DCollection(
            vertices[faces],
            facecolors=(0.65, 0.65, 0.65, 0.65),
            edgecolors=(0, 0, 0, 0.1),
            linewidths=0.2,
        )
    )
    axes.plot(*original.T, "k.--", label="Retained old ramp priors")
    profile = figure.add_subplot(1, 2, 2)
    length = np.r_[
        0.0, np.cumsum(np.linalg.norm(np.diff(original[:, :2], axis=0), axis=1))
    ]
    profile.plot(length, original[:, 2], "k.--", label="Retained old ramp priors")
    for i, xyz in enumerate(endpoints):
        label = "Actual body support" if i == 0 else None
        axes.plot(*xyz.T, color="tab:blue", linewidth=2, label=label)
        profile.plot(
            support["station_bounds_m"][i],
            xyz[:, 2],
            color="tab:blue",
            linewidth=2,
            label=label,
        )
    first_jump = True
    for i, kind in enumerate(support["connections"]):
        if kind != "body_step":
            continue
        xyz = np.vstack([endpoints[i, 1], endpoints[i + 1, 0]])
        axes.plot(*xyz.T, color="tab:orange", linewidth=2)
        profile.plot(
            [support["station_bounds_m"][i, 1]] * 2,
            xyz[:, 2],
            color="tab:orange",
            linewidth=2,
            label="One-sided body step" if first_jump else None,
        )
        first_jump = False
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    axes.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        zlim=(low[2], high[2]),
        xlabel="east / m",
        ylabel="north / m",
        zlabel="height / m",
        title="Float32 body / retained source path",
    )
    axes.set_box_aspect(high - low)
    axes.view_init(elev=32, azim=-60)
    profile.set(
        xlabel="original source path arclength / m",
        ylabel="height / m",
        title="Body-owned support; no fitted ramp between treads",
    )
    profile.grid(alpha=0.2)
    profile.legend()
    figure.suptitle(
        "Finite support reference; physical walking and native Nav admission remain separate"
    )
    figure.tight_layout()
    figure.savefig(output / "body-support.png", dpi=160, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 1000:
        parser.error("bounded positive repeat count required")
    args.output.mkdir(parents=True, exist_ok=True)
    start_load = os.getloadavg()
    began = time.perf_counter()
    capture = json.loads(FIXTURE.read_text())
    case = construct(capture)
    body = dict(case["data"])
    body["body_vertices"] = body["body_vertices"].astype(np.float32)
    original = np.array([r[:3] for r in capture["stations_east_north_grade_node"]])
    if args.reverse:
        original = original[::-1]
    path = original[:, :2]
    preparation = time.perf_counter() - began
    results, reports, timing = compare(body, path, args.repeats)
    scalar, batch = results["scalar"], results["batch"]
    for key, value in scalar.items():
        if isinstance(value, np.ndarray) and value.tobytes() != batch[key].tobytes():
            raise ValueError(f"batching changed support array {key}")
    if scalar["connections"] != batch["connections"]:
        raise ValueError("batching changed body junctions")
    node_ids = (
        capture["end_world_node_ids"][::-1]
        if args.reverse
        else capture["end_world_node_ids"]
    )
    ports = []
    for end, node_id in enumerate(node_ids):
        port = next(p for p in case["ports"] if p["world_node_id"] == node_id)
        ports.append(
            dict(
                port,
                support_span_id=0 if end == 0 else len(batch["body_cap_face_ids"]) - 1,
                support_endpoint=end,
                owned_capture_port=1 - end if args.reverse else end,
            )
        )
    report = {
        "reverse": args.reverse,
        "body_preparation_s": preparation,
        "scalar": reports["scalar"],
        "batch": reports["batch"],
        "timing": timing,
        "start_load": start_load,
        "step_junctions": batch["connections"].count("body_step"),
        "body_float32_position_copy_bytes": body["body_vertices"].nbytes,
        "process_peak_rss_bytes_before_export_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "scope": "Complete trace after separate body preparation; separate tracing, already imported libraries and other host work. Selected copy counters are not all allocations. No whole walking trajectory, native cache/Nav or device admission.",
    }
    np.savez(args.output / "body.npz", **body)
    np.savez(
        args.output / "support.npz",
        **{k: v for k, v in batch.items() if isinstance(v, np.ndarray)},
    )
    bindings = {
        "captured_source": capture,
        "source_instances_in_mask_bit_order": case["plan"]["rows"],
        "ports": ports,
        "connections": batch["connections"],
        "position_encoding": "float32 body vertices; binary64 source axis and barycentric weights",
        "support_height_source": "Immutable body cap geometry; owned World-node/gate heights remain in captured_source",
    }
    for name, value in (("report", report), ("source-bindings", bindings)):
        (args.output / (name + ".json")).write_text(json.dumps(value, indent=2) + "\n")
    if args.render:
        display(body, batch, original, args.output)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
