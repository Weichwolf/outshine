"""Compare two explicit terrain boundaries using the same source road and budgets."""

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_contact import ContactLimits
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_interior import relax_contact
from ground_rim_contact import build_rim_contact
from ground_rim_sampling import RimRecipe
from ground_rim_validation import compare_domains
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable
from replay_ground_rim import signature


def stage(context, boundary):
    cpu, wall = time.process_time(), time.perf_counter()
    queries = {"calls": 0, "points": 0}

    def sample(xy):
        queries["calls"] += 1
        queries["points"] += len(xy)
        return context.sample_z(xy)

    base = build_rim_contact(
        context.road,
        context.road_dem_z,
        sample,
        limits=ContactLimits(margins_m=(15.0,)),
        recipe=RimRecipe(boundary=boundary),
    )
    if base["status"] != "candidate":
        raise ValueError("Terrain boundary Pending: " + str(base["candidates"]))
    result = relax_contact(context.road, base, sample, maximum_sweeps=2)
    quality = qualify_contact(context.road, result)
    cost = {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
    return result, quality, cost, queries


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    options = (
        {} if args.seed is None else {"seed": tuple(args.seed), "geometry_recipe": None}
    )
    context = load_context(**options)
    results = {name: stage(context, name) for name in ("axis", "oriented")}
    expected = {name: signature(value[0]) for name, value in results.items()}
    samples = {name: [] for name in results}
    for turn in range(args.rounds):
        for name in ("axis", "oriented") if turn % 2 == 0 else ("oriented", "axis"):
            value, _, cost, queries = stage(context, name)
            if signature(value) != expected[name] or queries != results[name][3]:
                raise ValueError("Timed geometry/query work changed")
            samples[name].append(cost)
    traces = {}
    if args.trace:
        import memray

        for name in results:
            path = args.output / (name + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _, _, _ = stage(context, name)
            if signature(value) != expected[name]:
                raise ValueError("Traced geometry changed")
            traces[name] = allocations(path)
    validation, arrays = compare_domains(
        context, {name: value[0] for name, value in results.items()}
    )
    arrays.update(
        road_vertices=context.road.vertices, road_faces=context.road.triangles
    )
    methods = {}
    for name, (result, quality, _, queries) in results.items():
        for key in ("mesh", "coarse_mesh"):
            arrays[name + "_" + key + "_vertices"] = result[key].vertices
            arrays[name + "_" + key + "_faces"] = result[key].triangles
        for key in ("road_vertex_map", "prior_z", "outer_vertex_ids"):
            arrays[name + "_" + key] = result[key]
        methods[name] = {
            "artifact_sha256": expected[name],
            "quality": quality,
            "domain_aabb_area_m2": float(result["domain"].envelope.area),
            "domain_perimeter_m": float(result["domain"].length),
            "candidates": result["candidates"],
            "rim_profile": result["rim_profile"],
            "refinement": result["refinement"],
            "interior_fit": result["interior_fit"],
            "dem_queries": queries,
            "runs": samples[name],
            "allocations": traces.get(name),
            "fine_mesh_payload_bytes": result["mesh"].vertices.nbytes
            + result["mesh"].triangles.nbytes,
        }
    peak, peak_source = process_peak()
    np.savez_compressed(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/oriented-terrain-rim-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "inputs": context.metadata,
        "source_preparation_cpu_s": context.preparation_cpu_s,
        "source_state": context.report["state"],
        "source_binding": context.report["binding"],
        "methods": methods,
        "validation": validation,
        "process_peak_rss_bytes_before_plot": peak,
        "process_peak_source": peak_source,
        "stored_array_payload_bytes": sum(value.nbytes for value in arrays.values()),
        "scope": (
            "Local C0 recipes with different exact four-corner boundaries; same source road, "
            "15m margin,64 rim/128 free vertices,2 sweeps,soil grade2/3,2m rim adjustment. "
            "Timed/profiled domain+DEM+layout+fit+refinement+whole-output qualification; "
            "source preparation/validation/export/plot/profiler processing excluded. "
            "Allocation bytes are not copied bytes; total copies unmeasured. "
            "Smaller polygon area does not prove smaller axis-aligned index demand or IO. "
            "No surrounding terrain/Source/World/native/device/frame-budget admission."
        ),
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda value: value.tolist())
        + "\n"
    )
    if args.plot:
        from ground_rim_plot import draw_domains

        draw_domains(context, results, arrays, args.output)
    print(json.dumps({"commit": report["commit"], "validation": validation}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--seed", type=int, nargs=2, help="Actual chain edge PathId/SegmentOrdinal"
    )
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("--rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
