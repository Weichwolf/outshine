"""Same qualified road/DEM: general versus coverage-union terrain contacts (#1574)."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from ground_contact import build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable


def signature(result):
    value = hashlib.sha256()
    for key in ("mesh", "coarse_mesh"):
        value.update(result[key].vertices.tobytes())
        value.update(result[key].triangles.tobytes())
    value.update(result["road_vertex_map"].tobytes())
    value.update(result["prior_z"].tobytes())
    return value.hexdigest()


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context = load_context()
    road_before = context.road.vertices.tobytes() + context.road.triangles.tobytes()

    def stage(method):
        cpu, wall = time.process_time(), time.perf_counter()
        queries = {"calls": 0, "points": 0}

        def sample(xy):
            queries["calls"] += 1
            queries["points"] += len(xy)
            return context.sample_z(xy)

        result = build_contact(context.road, context.road_dem_z, sample, method=method)
        if result["status"] != "candidate":
            raise ValueError("Actual whole terrain contact remains Pending")
        checked = qualify_contact(context.road, result)
        result["dem_query_work"] = queries
        return (
            result,
            checked,
            {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall},
        )

    results = {m: stage(m) for m in ("general", "coverage")}
    expected = signature(results["coverage"][0])
    if any(signature(v[0]) != expected for v in results.values()):
        raise ValueError("Actual equal-quality union alternatives differ")
    samples = {m: [] for m in results}
    for turn in range(args.rounds):
        for method in tuple(results) if turn % 2 == 0 else tuple(reversed(results)):
            value, _, cost = stage(method)
            if signature(value) != expected:
                raise ValueError("Terrain contact geometry changed between runs")
            samples[method].append(cost)
    peak, peak_source = process_peak()
    traces = {}
    if args.trace:
        import memray

        for method in results:
            path = args.output / f"{method}.bin"
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _, _ = stage(method)
            if signature(value) != expected:
                raise ValueError("Profiled contact geometry changed")
            traces[method] = allocations(path)
            traces[method]["scope"] = (
                "Warm bounded contact variants, actual terrain construction/refinement and independent whole-output checks; source/road preparation and profiler processing excluded. Native and Python allocation requests, not copied bytes."
            )
    if (
        context.road.vertices.tobytes() + context.road.triangles.tobytes()
        != road_before
    ):
        raise ValueError("Original qualified road was mutated")
    chosen = results["coverage"][0]
    arrays = {
        "road_vertices": context.road.vertices,
        "road_faces": context.road.triangles,
        "vertices": chosen["mesh"].vertices,
        "faces": chosen["mesh"].triangles,
        "coarse_vertices": chosen["coarse_mesh"].vertices,
        "coarse_faces": chosen["coarse_mesh"].triangles,
        "prior_z": chosen["prior_z"],
        "road_vertex_map": chosen["road_vertex_map"],
        "outer_vertices": chosen["outer_vertex_ids"],
        "refinement_roots": chosen["refinement_roots"],
    }
    np.savez(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/ground-contact-comparison/1",
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
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "inputs": context.metadata,
        "anchor_longitude_latitude": context.anchor,
        "source_road_state": context.report["state"],
        "source_road_binding": context.report["binding"],
        "source_road_quality": context.report["terminal_quality"],
        "source_road_terminals": context.report["terminal_values"],
        "original_qualified_road_sha256": hashlib.sha256(road_before).hexdigest(),
        "source_road_preparation_cpu_s": context.preparation_cpu_s,
        "limits": asdict(chosen["limits"]),
        "artifact_sha256": expected,
        "process_peak_rss_bytes_before_profile_plot": peak,
        "process_peak_source": peak_source,
        "methods": {
            m: {
                "candidates": v["candidates"],
                "refinement": v["refinement"],
                "quality": q,
                "runs": samples[m],
                "allocations": traces.get(m),
                "corner_dem_observations": 4 * len(v["candidates"]),
                "corner_pin_pairs": sum(
                    c.get("corner_height_witness", {}).get("pairs", 0)
                    for c in v["candidates"]
                ),
                "layout_attempts": sum("work" in c for c in v["candidates"]),
                "dem_query_work": v["dem_query_work"],
                "refinement_quality_queries": v["refinement"]["quality_queries"],
                "output_mesh_payload_bytes": v["mesh"].vertices.nbytes
                + v["mesh"].triangles.nbytes,
                "stored_npz_array_payload_bytes": sum(
                    a.nbytes for a in arrays.values()
                ),
            }
            for m, (v, q, _) in results.items()
        },
        "scope": chosen["scope"]
        + " Margins are an explicit finite recipe family, not a global minimum-footprint or infeasibility proof. The5m whole-segment witness is used only with a matching physical road-gradient bound; a10m fixed-cell failure says nothing about other triangulations. Strict final edge/coverage checks supersede the retained helper's1e-8 XY diagnostic. Timing includes variants, bound DEM queries, terrain refinement and final checks. Source decode/road fit, traces/plots/NPZ are separate; total copied bytes/native cache/device/frame p99 remain unmeasured. Terrain is C0 and the four outer profiles are chosen local DEM chords, not original-grid or full-neighbour mesh contacts.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda v: v.tolist()) + "\n"
    )
    if args.plot:
        from ground_contact_plot import draw

        draw(context, chosen, args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "quality": results["coverage"][1],
                "methods": {
                    m: {
                        "cpu_s": [r["cpu_s"] for r in samples[m]],
                        "work": report["methods"][m]["refinement"],
                    }
                    for m in results
                },
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
        parser.error("--rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
