"""Compare unchanged World-stamp range work; not a Tokyo or whole-terrain profile."""

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
from earthwork_height_ranges import Stamp, direct, prepare
from world_stair_reference import FIXTURE, construct


def world_stamps():
    case = construct(json.loads(FIXTURE.read_text()))
    data = case["data"]
    triangles = data["body_vertices"][data["bottom_faces"]]
    stamps = []
    for triangle in triangles:
        o, p, q = (tuple(map(float, point)) for point in triangle)
        ae, an, az = (p[i] - o[i] for i in range(3))
        be, bn, bz = (q[i] - o[i] for i in range(3))
        determinant = ae * bn - an * be
        if determinant <= 0:
            raise ValueError("qualified positive underside cap required")
        stamps.append(
            Stamp(
                tuple(tuple(map(float, point[:2])) for point in triangle),
                o[0],
                o[1],
                o[2],
                (az * bn - an * bz) / determinant,
                (ae * bz - az * be) / determinant,
            )
        )
    return case, tuple(stamps)


def probes(stamps, spacing=0.5, reach=3.0):
    rings = np.asarray([s.ring for s in stamps])
    low, high = rings.min(axis=(0, 1)) - reach, rings.max(axis=(0, 1)) + reach
    x, y = np.meshgrid(
        np.arange(low[0], high[0] + spacing, spacing),
        np.arange(low[1], high[1] + spacing, spacing),
    )
    grid = np.column_stack([x.ravel(), y.ravel()])
    ids, points = [], []
    for index, ring in enumerate(rings):
        candidate = grid[
            np.all(grid >= ring.min(axis=0) - reach, axis=1)
            & np.all(grid <= ring.max(axis=0) + reach, axis=1)
        ]
        ids.extend([index] * len(candidate))
        points.extend(map(tuple, candidate.tolist()))
    if len(ids) > 65536:
        raise ValueError("bounded controlled stamp probes required")
    return np.asarray(ids, dtype=np.uint32), tuple(points)


def evaluate(stamps, ids, points, strategy):
    table = prepare(stamps) if strategy == "prepared" else None
    values = np.fromiter(
        (
            table[i].at(p) if table is not None else direct(stamps[i], p)
            for i, p in zip(ids, points, strict=True)
        ),
        dtype=np.float64,
        count=len(ids),
    )
    return values, table


def compare(stamps, ids, points, repeats):
    samples = {"direct": [], "prepared": []}
    for i in range(repeats + 1):
        for strategy in samples if i % 2 == 0 else reversed(samples):
            gc.collect()
            wall, cpu = time.perf_counter(), time.process_time()
            result = evaluate(stamps, ids, points, strategy)
            elapsed_cpu = time.process_time() - cpu
            samples[strategy].append((time.perf_counter() - wall, elapsed_cpu))
            del result
    outputs, reports = {}, {}
    for strategy, times in samples.items():
        gc.collect()
        tracemalloc.start()
        result = evaluate(stamps, ids, points, strategy)
        _current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        outputs[strategy] = result[0]
        reports[strategy] = {
            "first_wall_cpu_s": times[0],
            "following_wall_cpu_s": times[1:],
            "median_wall_cpu_s": np.median(times[1:], axis=0).tolist(),
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": blocks,
        }
    if outputs["direct"].tobytes() != outputs["prepared"].tobytes():
        raise ValueError("prepared range changed height words")
    return outputs, reports


def display(case, stamps, ids, points, direct_values, prepared_values, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    data = case["data"]
    figure = plt.figure(figsize=(14, 5))
    axes = figure.add_subplot(1, 3, 1, projection="3d")
    triangles = data["body_vertices"][data["bottom_faces"]]
    axes.add_collection3d(Poly3DCollection(triangles, alpha=0.65, edgecolors="gray"))
    xyz = triangles.reshape(-1, 3)
    low, high = xyz.min(axis=0), xyz.max(axis=0)
    axes.set(xlim=(low[0], high[0]), ylim=(low[1], high[1]), zlim=(low[2], high[2]))
    axes.set_box_aspect(high - low)
    axes.set_title("Actual fixed World body underside")
    points = np.asarray(points)
    raw = np.array([stamps[i].wants_at(p) for i, p in zip(ids, points, strict=True)])
    error = abs(prepared_values - direct_values)
    for index, (values, title) in enumerate(
        (
            (error, "Prepared − direct: exactly zero"),
            (abs(raw - direct_values), "Unclamped negative control / m"),
        ),
        start=2,
    ):
        ax = figure.add_subplot(1, 3, index)
        drawing = ax.scatter(
            *points.T, c=values, s=4, vmin=0, vmax=max(1, values.max())
        )
        ax.set_aspect("equal")
        ax.set(title=title, xlabel="east / m", ylabel="north / m")
        figure.colorbar(drawing, ax=ax)
    figure.suptitle(
        "Per-stamp probe results; not a deformed DEM or native terrain rendering"
    )
    figure.tight_layout()
    figure.savefig(output / "height-ranges.png", dpi=160)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error("bounded positive repeat count required")
    args.output.mkdir(parents=True, exist_ok=True)
    load = os.getloadavg()
    began = time.perf_counter()
    case, stamps = world_stamps()
    preparation = time.perf_counter() - began
    ids, points = probes(stamps)
    small = (
        np.arange(len(stamps), dtype=np.uint32),
        tuple(tuple(np.mean(s.ring, axis=0)) for s in stamps),
    )
    results, reports = {}, {}
    for name, (query_ids, query_points) in (
        ("one_per_stamp", small),
        ("local_grid_candidates", (ids, points)),
    ):
        outputs, timings = compare(stamps, query_ids, query_points, args.repeats)
        results[name] = outputs
        reports[name] = {
            "queries": len(query_ids),
            "direct_wants_at_evaluations": len(query_ids) * 4,
            "prepared_wants_at_evaluations_including_preparation": len(query_ids)
            + sum(len(s.ring) for s in stamps),
            "timing_includes_range_preparation": True,
            **timings,
        }
    overhead = []
    for _ in range(101):
        wall, cpu = time.perf_counter(), time.process_time()
        elapsed_cpu = time.process_time() - cpu
        overhead.append((time.perf_counter() - wall, elapsed_cpu))
    report = {
        "body_and_stamp_construction_s": preparation,
        "stamps": len(stamps),
        "range_numeric_payload_bytes": 16 * len(stamps),
        "ring_geometry_copied_by_range_preparation_bytes": 0,
        "start_load": load,
        "timer_pair_median_wall_cpu_s": np.median(overhead, axis=0).tolist(),
        "process_peak_rss_bytes_before_export_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "workloads": reports,
        "scope": "Existing fixed World body, explicit ground recipe, non-profiled stamps. Controlled 0.5 m grid candidates in per-stamp boxes expanded 3 m; no DEM/Tokyo profile, broadphase cost or complete terrain decision. Imported libraries; loaded host. Ranges include preparation each invocation. Python traces are not all native allocations or UMA.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(args.output / "body.npz", **case["data"])
    np.savez(
        args.output / "probes.npz",
        stamp_ids=ids,
        points=points,
        **results["local_grid_candidates"],
    )
    if args.render:
        display(
            case,
            stamps,
            ids,
            points,
            direct_values=results["local_grid_candidates"]["direct"],
            prepared_values=results["local_grid_candidates"]["prepared"],
            output=args.output,
        )
    print(json.dumps(report))


if __name__ == "__main__":
    main()
