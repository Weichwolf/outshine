"""Same immutable roads/rims and fixed XY controls for bounded edge probe choices."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_contact import build_contact
from ground_contact_checks import qualify_contact
from ground_edge_probe_inputs import prepare
from ground_rim_validation import compare_domains
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable

METHODS = ("midpoint", "centroid", "quartiles", "octiles")
ARRAYS = ("road_vertex_map", "outer_vertex_ids", "prior_z", "refinement_roots")


def stage(contexts, method):
    began, wall = time.process_time(), time.perf_counter()
    results, queries = {}, {"calls": 0, "points": 0}
    for key, context in contexts.items():

        def sample(xy, context=context):
            queries["calls"] += 1
            queries["points"] += len(xy)
            return context.sample_z(xy)

        result = build_contact(
            context.road,
            context.road_dem_z,
            sample,
            refinement_method=method,
        )
        if result["status"] != "candidate":
            raise ValueError(
                "Whole original contact retained: " + key + ":" + result["reason"]
            )
        results[key] = {
            "result": result,
            "quality": qualify_contact(context.road, result),
        }
    return results, {
        "cpu_s": time.process_time() - began,
        "wall_s": time.perf_counter() - wall,
        "dem_queries": queries,
    }


def fingerprint(results):
    digest = hashlib.sha256()
    for key, row in sorted(results.items()):
        digest.update(key.encode())
        result = row["result"]
        for mesh in (result["mesh"], result["coarse_mesh"]):
            digest.update(mesh.vertices.tobytes())
            digest.update(mesh.triangles.tobytes())
        for name in ARRAYS:
            digest.update(result[name].tobytes())
    return digest.hexdigest()


def export(contexts, results, output):
    arrays, records = {}, {}
    for key, context in contexts.items():
        arrays[key + "_road_vertices"] = context.road.vertices
        arrays[key + "_road_faces"] = context.road.triangles
    for method, rows in results.items():
        records[method] = {}
        for key, row in rows.items():
            result = row["result"]
            prefix = method + "_" + key + "_"
            for name in ARRAYS:
                arrays[prefix + name] = result[name]
            for name in ("mesh", "coarse_mesh"):
                arrays[prefix + name + "_vertices"] = result[name].vertices
                arrays[prefix + name + "_faces"] = result[name].triangles
            records[method][key] = {
                "array_prefix": prefix,
                "quality": row["quality"],
                "refinement": result["refinement"],
                "candidates": result["candidates"],
                "payload_bytes": result["mesh"].vertices.nbytes
                + result["mesh"].triangles.nbytes,
            }
    validation = {}
    for key, context in contexts.items():
        values = {name: results[name][key]["result"] for name in METHODS}
        report, controls = compare_domains(context, values)
        validation[key] = report
        arrays.update({key + "_" + name: value for name, value in controls.items()})
    np.savez_compressed(output / "geometry.npz", **arrays)
    return records, validation


def replay(args):
    dirty = subprocess.check_output(["git", "status", "--porcelain"], text=True)
    if dirty:
        raise ValueError("Freeze complete edge-probe source before evidence")
    args.output.mkdir(parents=True, exist_ok=False)
    contexts, acquisition = prepare()
    results = {method: stage(contexts, method)[0] for method in METHODS}
    hashes = {method: fingerprint(rows) for method, rows in results.items()}
    runs, traces = {method: [] for method in METHODS}, {}
    for turn in range(args.rounds):
        for method in METHODS if turn % 2 == 0 else reversed(METHODS):
            rows, cost = stage(contexts, method)
            if fingerprint(rows) != hashes[method]:
                raise ValueError("Timed whole contact changed")
            runs[method].append(cost)
    if args.trace:
        import memray

        for method in METHODS:
            path = args.output / (method + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                rows, _ = stage(contexts, method)
            if fingerprint(rows) != hashes[method]:
                raise ValueError("Profiled whole contact changed")
            traces[method] = allocations(path) | {
                "scope": "Warm complete three-original contact construction, all DEM queries, topology/refinement and whole contact checks; output retained. Source/road acquisition, profiling processing, fixed-grid validation/export/plot excluded. Allocation bytes are not copied bytes."
            }
    records, validation = export(contexts, results, args.output)
    peak, peak_source = process_peak()
    report = {
        "schema": "outshine/terrain-edge-probe-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": dirty,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "source_preparation_cpu_s": acquisition,
        "inputs": {key: context.metadata for key, context in contexts.items()},
        "methods": {
            method: {
                "artifact_sha256": hashes[method],
                "runs": runs[method],
                "allocations": traces.get(method),
                "cases": records[method],
            }
            for method in METHODS
        },
        "same_fixed_xy_validation": validation,
        "process_peak_rss_bytes_before_plot": peak,
        "process_peak_source": peak_source,
        "scope": "Unchanged source roads and four chosen outer DEM chords, same contact/detail capacities. New edge methods count candidate and quality target queries together and cap expansion before allocation. Fixed-grid controls are sampled observations, not a continuous DEM bound. No original-grid/neighbour/Source/World/native/device/frame-p99/8GB admission. Total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda v: v.tolist()) + "\n"
    )
    if args.plot:
        from ground_edge_probe_plot import draw

        draw(args.output)
    print(
        json.dumps(
            {
                method: {
                    key: row["result"]["refinement"]["new_vertices"]
                    for key, row in rows.items()
                }
                for method, rows in results.items()
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("Bounded rounds1..16 required")
    replay(args)


if __name__ == "__main__":
    main()
