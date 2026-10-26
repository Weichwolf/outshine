"""Fresh-worker solver comparison and actual shared stair product inspection."""

import argparse
import hashlib
import json
import os
import resource
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from mvt_stair_t_junction import reproduce
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_display import display
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links


def run(
    solver,
    *,
    output=None,
    trace_memory=False,
    formulation="projected",
    input_path=FIXTURE,
    region_sources=None,
):
    if trace_memory:
        tracemalloc.start()
    began = time.perf_counter()
    case, sources = inputs(input_path)
    if region_sources is None:
        proposal = plan_connections(sources)
    else:
        from mvt_t_profile_region import region_inputs

        case, sources, proposal = region_inputs(case, region_sources)
    connected = time.perf_counter()
    plan = prepare_profiles(case, sources, proposal, mesh_recipe="outward_binary_grid")
    prepared = time.perf_counter()
    heights, solve = solve_profiles(plan, solver=solver, height_formulation=formulation)
    solved = time.perf_counter()
    if heights is None:
        raise ValueError(solve)
    body, data, geometry = build(plan, heights)
    built = time.perf_counter()
    proof = audit_profiles(plan, heights, data)
    vertex_links(body.vertices, body.triangles)
    validated = time.perf_counter()
    products = shared_products(plan, heights, data)
    produced = time.perf_counter()
    if trace_memory:
        _current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    else:
        peak, blocks = None, None
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report = {
        "solver": solver,
        "instrumented": trace_memory,
        "solve": solve,
        "geometry": geometry,
        "proof": proof,
        "layout": plan["note"],
        "station_refinement": plan["station_refinement"],
        **(
            {"region_work": plan["region_context"]["work"]}
            if "region_context" in plan
            else {}
        ),
        "connection_work": proposal["work"],
        "footprint_checks": plan["footprint_checks"],
        "landing_displacement_m": plan["landing_displacement_m"],
        "seconds": {
            "input_connections": connected - began,
            "surface_prepare": prepared - connected,
            "height_fit": solved - prepared,
            "body_build": built - solved,
            "validation": validated - built,
            "products": produced - validated,
            "total": produced - began,
        },
        "python_traced_peak_bytes": peak,
        "python_traced_live_blocks": blocks,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        "memory_scope": "whole fresh worker including imports and GEOS; traced blocks are live, not total allocations",
        "payload_bytes": {
            "surface_xy_heights": plan["xy"].nbytes + heights.nbytes,
            "exported_body_arrays": sum(a.nbytes for a in data.values()),
            "products_json": len(json.dumps(products).encode()),
        },
        "explicit_copy_bytes": {
            "quantized_xy_f32_then_f64": plan["xy"].size * 12,
            "source_sampler_xyz_constructions": len(plan["rows"]) * len(heights) * 24,
        },
        "copy_scope": "only these explicit array paths; not complete allocator, native, IO or GPU traffic",
        "counts": {
            "sources": len(sources),
            "surface_vertices": len(plan["xy"]),
            "surface_faces": sum(map(len, plan["parts"])),
            "interface_edges": len(products["interface_ports"]),
            "navigation_nodes": len(products["navigation"]["nodes_xyz_m"]),
            "navigation_edges": len(products["navigation"]["edges"]),
            "external_profiles": len(products["external_contacts"]),
        },
    }
    if output is not None:
        arrays = {
            "xy": plan["xy"],
            "heights": heights,
            **data,
            **{f"source_faces_{i}": f for i, f in enumerate(plan["parts"])},
        }
        np.savez_compressed(output / "products.npz", **arrays)
        (output / "shared-plan.json").write_text(json.dumps(products, indent=2) + "\n")
        display(plan, heights, data, products, output / "profiles.png")
    return report


def compare(output, *, input_path=FIXTURE, region_sources=None):
    output.mkdir(parents=True, exist_ok=True)
    results = {}
    worker = [sys.executable, str(Path(__file__).resolve()), "--input", str(input_path)]
    if region_sources is not None:
        worker += ["--region-sources", *region_sources]
    for solver in ("highs-ds", "highs-ipm"):
        cmd = worker + ["--worker", solver]
        if solver == "highs-ds":
            cmd += ["--output", str(output)]
        completed = subprocess.run(
            cmd, capture_output=True, text=True, check=True, env=os.environ.copy()
        )
        results[solver] = json.loads(completed.stdout)
        print(json.dumps({"solver": solver, "result": results[solver]}), flush=True)
    cmd = worker + [
        "--worker",
        "highs-ds",
        "--trace-memory",
    ]
    measured = subprocess.run(
        cmd, capture_output=True, text=True, check=True, env=os.environ.copy()
    )
    results["memory_run"] = json.loads(measured.stdout)
    explicit = subprocess.run(
        worker
        + [
            "--worker",
            "highs-ds",
            "--formulation",
            "equalities",
        ],
        capture_output=True,
        text=True,
        check=True,
        env=os.environ.copy(),
    )
    results["equalities_control"] = json.loads(explicit.stdout)
    if (
        abs(
            results["highs-ds"]["solve"]["objective"]
            - results["highs-ipm"]["solve"]["objective"]
        )
        > 1e-7
    ):
        raise ValueError("independent algorithms differ on the same finite objective")
    if (
        abs(
            results["equalities_control"]["solve"]["objective"]
            - results["highs-ds"]["solve"]["objective"]
        )
        > 1e-7
    ):
        raise ValueError(
            "equivalent height formulations differ on the finite objective"
        )
    case, _rows = inputs(input_path)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": hashlib.sha256(input_path.read_bytes()).hexdigest(),
        **(
            {"baseline_frozen_native": reproduce(case)}
            if region_sources is None
            else {
                "capture_provenance": case["provenance"],
                "region_sources": region_sources,
            }
        ),
        "results": results,
        "scope": "local 2.5D proposal; outer contacts, native consumers and device admission remain open",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--worker", choices=("highs-ds", "highs-ipm"))
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--region-sources", nargs="+")
    parser.add_argument(
        "--formulation", choices=("projected", "equalities"), default="projected"
    )
    args = parser.parse_args()
    if args.worker:
        print(
            json.dumps(
                run(
                    args.worker,
                    output=args.output,
                    trace_memory=args.trace_memory,
                    formulation=args.formulation,
                    input_path=args.input,
                    region_sources=args.region_sources,
                )
            )
        )
    elif args.output is not None:
        compare(args.output, input_path=args.input, region_sources=args.region_sources)
    else:
        parser.error("an explicit output directory is required")


if __name__ == "__main__":
    main()
