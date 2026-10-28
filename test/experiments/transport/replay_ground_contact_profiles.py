"""Whole-profile preflight versus mesh rejection on identical narrow contacts."""

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
from ground_contact import ContactLimits, build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable
from replay_ground_contact import signature


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context = load_context()
    before = context.road.vertices.tobytes() + context.road.triangles.tobytes()
    tight = ContactLimits(margins_m=(5.0, 10.0, 10.5, 11.0, 12.0, 15.0, 20.0))
    variants = {
        "baseline": (ContactLimits(), False),
        "tight_unfiltered": (tight, False),
        "tight_guarded": (tight, True),
    }

    def stage(name):
        cpu, wall = time.process_time(), time.perf_counter()
        queries = {"calls": 0, "points": 0}

        def sample(xy):
            queries["calls"] += 1
            queries["points"] += len(xy)
            return context.sample_z(xy)

        limits, guarded = variants[name]
        result = build_contact(
            context.road,
            context.road_dem_z,
            sample,
            limits=limits,
            whole_profiles=guarded,
        )
        if result["status"] != "candidate":
            raise ValueError("Actual contact did not qualify")
        quality = qualify_contact(context.road, result)
        result["dem_query_work"] = queries
        cost = {
            "cpu_s": time.process_time() - cpu,
            "wall_s": time.perf_counter() - wall,
        }
        return result, quality, cost

    results = {name: stage(name) for name in variants}
    signatures = {name: signature(value[0]) for name, value in results.items()}
    if signatures["tight_guarded"] != signatures["tight_unfiltered"]:
        raise ValueError("Equal-quality guarded/unfiltered geometry differs")
    samples = {name: [] for name in variants}
    for turn in range(args.rounds):
        for name in tuple(variants) if turn % 2 == 0 else tuple(reversed(variants)):
            value, _, cost = stage(name)
            if signature(value) != signatures[name]:
                raise ValueError("Timed contact changed")
            samples[name].append(cost)
    peak, peak_source = process_peak()
    traces = {}
    if args.trace:
        import memray

        for name in variants:
            path = args.output / f"{name}.bin"
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _, _ = stage(name)
            if signature(value) != signatures[name]:
                raise ValueError("Traced contact changed")
            traces[name] = allocations(path) | {
                "scope": "Warm contact and independent output checks; preparation, plots, profiler processing excluded. Requested allocations are not copied bytes."
            }
    if before != context.road.vertices.tobytes() + context.road.triangles.tobytes():
        raise ValueError("Qualified source road changed")
    arrays = {
        "road_vertices": context.road.vertices,
        "road_faces": context.road.triangles,
    }
    methods = {}
    for name, (value, quality, _) in results.items():
        for key in ("mesh", "coarse_mesh"):
            arrays[f"{name}_{key}_vertices"] = value[key].vertices
            arrays[f"{name}_{key}_faces"] = value[key].triangles
        for key in (
            "road_vertex_map",
            "prior_z",
            "outer_vertex_ids",
            "refinement_roots",
        ):
            arrays[f"{name}_{key}"] = value[key]
        methods[name] = {
            "artifact_sha256": signatures[name],
            "limits": asdict(value["limits"]),
            "quality": quality,
            "runs": samples[name],
            "allocations": traces.get(name),
            "candidates": value["candidates"],
            "refinement": value["refinement"],
            "dem_query_work": value["dem_query_work"],
            "profile_proof_work": value.get("profile_proof_work"),
            "corner_pin_pairs": sum(
                c.get("corner_height_witness", {}).get("pairs", 0)
                for c in value["candidates"]
            ),
            "layout_attempts": sum("work" in c for c in value["candidates"]),
            "output_mesh_payload_bytes": value["mesh"].vertices.nbytes
            + value["mesh"].triangles.nbytes,
        }
    np.savez(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/whole-contact-profiles/1",
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
        "source_road_quality": context.report["terminal_quality"],
        "source_road_terminals": context.report["terminal_values"],
        "original_qualified_road_sha256": hashlib.sha256(before).hexdigest(),
        "source_road_preparation_cpu_s": context.preparation_cpu_s,
        "process_peak_rss_bytes_before_profile_plot": peak,
        "process_peak_source": peak_source,
        "stored_npz_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        "methods": methods,
        "scope": "Same actual123XYZ/154road faces/three source terminals/90 whole contacts. Baseline and tighter family change chosen outer DEM chord/domain; only tight_unfiltered versus tight_guarded are identical-quality geometry. Bounded exact negative witnesses do not certify feasibility, minimum area or all contradictions. Whole convex patch and uniform road gradient required. Final independent geometric checks still mandatory. Local C0 four-DEM-chord surface, no original native grid/neighbour/World/Source/device/cache/FPS proof. Preparation separate; total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2) + "\n"
    )
    if args.plot:
        from ground_contact_profiles_plot import draw

        draw(context, results["baseline"][0], results["tight_guarded"][0], args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "methods": {
                    name: {
                        "cpu_s": [s["cpu_s"] for s in samples[name]],
                        "domain_m2": value[1]["domain_area_m2"],
                        "layout_attempts": methods[name]["layout_attempts"],
                        "allocations": traces.get(name),
                    }
                    for name, value in results.items()
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
