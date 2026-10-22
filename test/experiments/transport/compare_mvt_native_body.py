"""Bounded native body: declared directional recipe and unchanged control gates."""

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


def orientation_counts(data):
    vertices, faces = data["body_vertices"], data["body_faces"]
    triangles = vertices[faces]
    cast = vertices.astype(np.float32).astype(float)[faces]
    before = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    after = np.cross(cast[:, 1] - cast[:, 0], cast[:, 2] - cast[:, 0])
    return {
        "faces": len(faces),
        "invalid_float32_faces": int(np.sum(np.einsum("ij,ij->i", before, after) <= 0)),
    }


def run(solver, partition_recipe="envelope_indexed_band_remesh"):
    began = time.perf_counter()
    case, sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    plan, contacts = prepare_native_body_plan(
        case,
        sources,
        plan_connections(sources),
        native,
        tread_partition_recipe=partition_recipe,
    )
    prepared = time.perf_counter()
    heights, solve = solve_profiles(
        plan,
        fixed_contacts=contacts,
        max_prior_change_m=3,
        gradient_model="axis_crossfall",
        solver=solver,
    )
    solved = time.perf_counter()
    if heights is None:
        raise ValueError(solve)
    body, data, geometry = build(plan, heights)
    built = time.perf_counter()
    result, field = consumed_plan(plan, data)
    proof = audit_profiles(
        result, field, data, max_prior_change_m=3, gradient_model="axis_crossfall"
    )
    fixed = audit_rendered_contacts(result, data, contacts)
    vertex_links(body.vertices, body.triangles)
    validated = time.perf_counter()
    products = shared_products(result, field, data)
    produced = time.perf_counter()
    plan["tread_partition_recipe"] = "indexed_levels"
    _baseline, baseline_data, baseline_geometry = build(plan, heights)
    baseline_fixed = audit_rendered_contacts(plan, baseline_data, contacts)
    compared = time.perf_counter()
    controls = []
    for model, cap in (("whole_norm", 3), ("axis_crossfall", 2)):
        _z, control = solve_profiles(
            plan,
            fixed_contacts=contacts,
            max_prior_change_m=cap,
            gradient_model=model,
            solver=solver,
        )
        controls.append(control)
    return (
        result,
        data,
        baseline_data,
        products,
        {
            "scope": "Local Python directional construction. Native outside mesh, navstation, terrain and device/Places admission remain open.",
            "solver": solver,
            "partition_recipe": partition_recipe,
            "solve": solve,
            "geometry": geometry,
            "profile_audit": proof,
            "rendered_native_contacts": fixed,
            "orientation": orientation_counts(data),
            "baseline": {
                "geometry": baseline_geometry,
                "orientation": orientation_counts(baseline_data),
                "rendered_native_contacts": baseline_fixed,
                "scope": "Same fitted field and native frontiers, indexed clipping without band retriangulation; actual Float32 validity is measured, not assumed.",
            },
            "controls": controls,
            "seconds": {
                "input_layout": prepared - began,
                "fit": solved - prepared,
                "body": built - solved,
                "validation": validated - built,
                "products": produced - validated,
                "candidate_total": produced - began,
                "baseline_comparison": compared - produced,
                "controls": time.perf_counter() - compared,
            },
            "selected_export_array_bytes": sum(a.nbytes for a in data.values()),
            "position_index_bytes": {
                "candidate": 12 * (len(body.vertices) + len(body.triangles)),
                "baseline": 12
                * (
                    len(baseline_data["body_vertices"])
                    + len(baseline_data["body_faces"])
                ),
                "scope": "Float32 XYZ and uint32 triangle-index arrays only; no complete native resource or upload measurement.",
            },
            "native_provenance": native["provenance"],
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--solver", choices=("highs-ds", "highs-ipm"), default="highs-ds"
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument(
        "--partition-recipe",
        choices=("indexed_band_remesh", "envelope_indexed_band_remesh"),
        default="envelope_indexed_band_remesh",
    )
    args = parser.parse_args()
    if args.trace_memory:
        tracemalloc.start()
    plan, data, baseline, products, report = run(args.solver, args.partition_recipe)
    if args.trace_memory:
        _current, report["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        report["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report["process_peak_rss_bytes"] = rss if sys.platform == "darwin" else rss * 1024
    report["instrumented"] = args.trace_memory
    report["memory_scope"] = (
        "Whole local comparison including controls and retained baseline; tracing excludes plotting/export. Live blocks are not all allocations; export bytes are not unique UMA residency."
    )
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    np.savez(args.output / "baseline.npz", **baseline)
    (args.output / "shared-plan.json").write_text(json.dumps(products, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(plan, data, args.output)
    print(
        json.dumps(
            {
                "orientation": report["orientation"],
                "geometry": {
                    k: report["geometry"][k] for k in ("vertices", "faces", "risers")
                },
                "scope": report["scope"],
            }
        )
    )


if __name__ == "__main__":
    main()
