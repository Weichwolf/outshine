"""Same road/domain: coarse chords, full/reduced quadratic fit and adaptive rim."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_contact import ContactLimits, build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_rim_contact import build_rim_contact
from ground_rim_sampling import RimRecipe
from ground_rim_validation import ordered_rim, validate_rim, validation_points
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable


def signature(result):
    value = hashlib.sha256()
    for name in ("mesh", "coarse_mesh"):
        value.update(result[name].vertices.tobytes())
        value.update(result[name].triangles.tobytes())
    value.update(result["prior_z"].tobytes())
    value.update(result["road_vertex_map"].tobytes())
    return value.hexdigest()


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context = load_context()
    road = context.road
    limits = ContactLimits(margins_m=(15.0,))
    original = road.vertices.tobytes() + road.triangles.tobytes()
    variants = {
        "coarse": (None, None),
        "uniform_full": (RimRecipe(), "full"),
        "uniform_reduced": (RimRecipe(), "reduced"),
        "adaptive_reduced": (RimRecipe(sampling="adaptive"), "reduced"),
    }

    def stage(name):
        cpu, wall = time.process_time(), time.perf_counter()
        queries = {"calls": 0, "points": 0}

        def sample(xy):
            queries["calls"] += 1
            queries["points"] += len(xy)
            return context.sample_z(xy)

        recipe, method = variants[name]
        if recipe is None:
            result = build_contact(road, context.road_dem_z, sample, limits=limits)
        else:
            result = build_rim_contact(
                road,
                context.road_dem_z,
                sample,
                limits=limits,
                recipe=recipe,
                fit_method=method,
            )
        if result["status"] != "candidate":
            raise ValueError("Actual rim did not qualify: " + str(result["candidates"]))
        quality = qualify_contact(road, result)
        return (
            result,
            quality,
            {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall},
            queries,
        )

    results = {name: stage(name) for name in variants}
    expected = {name: signature(v[0]) for name, v in results.items()}
    full, reduced = (results[name][0] for name in ("uniform_full", "uniform_reduced"))
    if not np.array_equal(
        full["mesh"].triangles, reduced["mesh"].triangles
    ) or not np.array_equal(
        full["mesh"].vertices[:, :2], reduced["mesh"].vertices[:, :2]
    ):
        raise ValueError("Equivalent full/reduced topology differs")
    difference = float(
        abs(full["mesh"].vertices[:, 2] - reduced["mesh"].vertices[:, 2]).max()
    )
    if difference > 1e-5:
        raise ValueError("Equivalent quadratic solutions disagree")
    samples = {name: [] for name in variants}
    for turn in range(args.rounds):
        for name in tuple(variants) if turn % 2 == 0 else tuple(reversed(variants)):
            result, _, cost, _ = stage(name)
            if signature(result) != expected[name]:
                raise ValueError("Timed geometry changed")
            samples[name].append(cost)
    at, xy = validation_points(results["coarse"][0])
    observed = context.sample_z(xy)
    validation = {}
    arrays = {
        "road_vertices": road.vertices,
        "road_faces": road.triangles,
        "validation_arc_m": at,
        "validation_xy": xy,
        "validation_dem_z": observed,
    }
    for name, (result, _, _, _) in results.items():
        validation[name], predicted = validate_rim(result, at, observed)
        arrays[name + "_validation_z"] = predicted
        for key in ("mesh", "coarse_mesh"):
            arrays[f"{name}_{key}_vertices"] = result[key].vertices
            arrays[f"{name}_{key}_faces"] = result[key].triangles
        for key in (
            "prior_z",
            "road_vertex_map",
            "outer_vertex_ids",
            "refinement_roots",
        ):
            arrays[name + "_" + key] = result[key]
        arrays[name + "_ordered_rim"] = ordered_rim(result)
    peak, peak_source = process_peak()
    traces = {}
    if args.trace:
        import memray

        for name in variants:
            path = args.output / (name + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _, _, _ = stage(name)
            if signature(value) != expected[name]:
                raise ValueError("Traced geometry changed")
            traces[name] = allocations(path) | {
                "scope": "Warm source-bound rim observations/layout/fit/refinement and independent complete-output checks; source preparation, unfitted validation/export/plot/profiler processing excluded. Allocation bytes are not copied bytes."
            }
    if original != road.vertices.tobytes() + road.triangles.tobytes():
        raise ValueError("Source road changed")
    np.savez(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/fitted-terrain-rim/1",
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
        "source_road_state": context.report["state"],
        "source_road_binding": context.report["binding"],
        "source_road_terminals": context.report["terminal_values"],
        "source_preparation_cpu_s": context.preparation_cpu_s,
        "original_road_sha256": hashlib.sha256(original).hexdigest(),
        "full_reduced_maximum_height_difference_m": difference,
        "process_peak_rss_bytes_before_profile_plot": peak,
        "process_peak_source": peak_source,
        "stored_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        "methods": {
            name: {
                "artifact_sha256": expected[name],
                "quality": q,
                "candidates": v["candidates"],
                "rim_profile": v.get("rim_profile"),
                "refinement": v["refinement"],
                "dem_query_work": queries,
                "validation": validation[name],
                "runs": samples[name],
                "allocations": traces.get(name),
                "mesh_payload_bytes": v["mesh"].vertices.nbytes
                + v["mesh"].triangles.nbytes,
            }
            for name, (v, q, _, queries) in results.items()
        },
        "scope": "Same15m rectangle and exact source road/three terminals/90 contacts/four DEM corners. New intermediate targets explicitly fitted within2m under the same whole soil grade. Full/reduced solve the same strictly convex weighted quadratic problem; approximate agreement, not asserted bit equality. Adaptive midpoint heuristic has independent measured errors, no continuous certificate. Coarse four-chord baseline has lower quality. No native original-grid/neighbour/C1/Source/World/cache/device/frame proof. Total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda v: v.tolist()) + "\n"
    )
    if args.plot:
        from ground_rim_plot import draw

        draw(context, results, at, observed, arrays, args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "methods": {
                    name: {
                        "cpu_ms": [r["cpu_s"] * 1000 for r in samples[name]],
                        "validation": validation[name],
                        "allocations": traces.get(name),
                    }
                    for name in variants
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
