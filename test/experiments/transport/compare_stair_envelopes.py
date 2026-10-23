"""Fresh-process body-construction cost with identical native inputs and audits."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from mvt_native_body_display import display
from mvt_native_body_layout import prepare_native_body_plan
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links


def run(recipe, trace):
    case, sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    plan, contacts = prepare_native_body_plan(
        case, sources, plan_connections(sources), native, tread_partition_recipe=recipe
    )
    heights, solve = solve_profiles(
        plan,
        fixed_contacts=contacts,
        max_prior_change_m=3,
        gradient_model="axis_crossfall",
    )
    if heights is None:
        raise ValueError(solve)
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    body, data, geometry = build(plan, heights)
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    result, field = consumed_plan(plan, data)
    audit = audit_profiles(
        result, field, data, max_prior_change_m=3, gradient_model="axis_crossfall"
    )
    fixed = audit_rendered_contacts(result, data, contacts)
    vertex_links(body.vertices, body.triangles)
    products = shared_products(result, field, data)
    return (
        result,
        data,
        products,
        {
            "recipe": recipe,
            "instrumented": trace,
            "body_build_s": elapsed,
            "geometry": geometry,
            "audit": audit,
            "rendered_native_contacts": fixed,
            "exported_array_bytes": sum(a.nbytes for a in data.values()),
            "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
            **memory,
            "scope": "Build-only time and Python tracing, after identical preparation/fit and before quality checks, products, plotting or export. RSS is the whole process high-water mark; traced live blocks are not all allocations. No native device claim.",
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--recipe",
        choices=("indexed_band_remesh", "envelope_indexed_band_remesh"),
        default="envelope_indexed_band_remesh",
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    plan, data, products, report = run(args.recipe, args.trace_memory)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    (args.output / "shared-plan.json").write_text(json.dumps(products, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(plan, data, args.output)
    print(
        json.dumps(
            {
                "recipe": args.recipe,
                "body_build_s": report["body_build_s"],
                "edge_queries": report["geometry"]["edge_queries"],
            }
        )
    )


if __name__ == "__main__":
    main()
