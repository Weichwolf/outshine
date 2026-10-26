"""Bound real native crosscuts without claiming a valid replacement body."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_mvt_t_profile_contacts import finite_prior_bound
from mvt_native_cut_layout import prepare_native_cut_plan
from mvt_stair_native_crosscuts import FIXTURE
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_contacts import add_contacts, contact_residuals, prepare_contacts
from mvt_t_profile_solve import solve_profiles
from scipy.optimize import linprog
from stair_junction_solve import Conditions


def inspect_layout():
    began = time.perf_counter()
    case, sources = inputs()
    native = json.loads(FIXTURE.read_text())
    plan, contacts = prepare_native_cut_plan(
        case, sources, plan_connections(sources), native
    )
    prepared = time.perf_counter()
    bindings, work = prepare_contacts(plan, contacts)
    system = Conditions(len(plan["xy"]))
    add_contacts(system, bindings, 30, row_scale=1)
    answer = linprog(
        np.zeros(system.variables),
        A_ub=system.matrix(),
        b_ub=system.bounds,
        bounds=[(None, None)] * system.variables,
        method="highs-ds",
    )
    if not answer.success:
        raise ValueError("real native contacts must first be mutually consistent")
    lower = {c["contact_id"]: finite_prior_bound(plan, c) for c in bindings}
    models = []
    for cap in (2, 3):
        _heights, note = solve_profiles(
            plan, fixed_contacts=contacts, max_prior_change_m=cap
        )
        models.append(note)
    arrays = {
        "xy": plan["xy"],
        **{f"source_faces_{i}": f for i, f in enumerate(plan["parts"])},
    }
    return (
        plan,
        arrays,
        {
            "scope": "Input/binding experiment only. No valid body, native replacement, navigation, terrain or frame admission.",
            "native_provenance": native["provenance"],
            "native_profiles": plan["native_cut_context"]["profiles"],
            "source_scopes": plan["native_cut_context"]["source_scopes"],
            "retained_original_source_ids": [r["source_id"] for r in sources],
            "contact_only": {
                "valid": True,
                "work": work,
                "height_errors_m": contact_residuals(bindings, answer.x + 30),
            },
            "necessary_finite_prior_bounds": lower,
            "whole_facet_hypothesis_models": models,
            "work": {
                "vertices": len(plan["xy"]),
                "source_faces": sum(map(len, plan["parts"])),
                "selected_geometry_array_bytes": sum(a.nbytes for a in arrays.values()),
                "layout": plan["note"],
                "station_refinement": plan["station_refinement"],
            },
            "seconds": {
                "input_and_layout": prepared - began,
                "total": time.perf_counter() - began,
            },
        },
    )


def display(plan, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(12, 7))
    for owner, (row, faces) in enumerate(zip(plan["rows"], plan["parts"], strict=True)):
        xy = plan["xy"]
        axes[0].triplot(xy[:, 0], xy[:, 1], faces, linewidth=0.3, alpha=0.7)
        at = row["xy"].mean(axis=0)
        axes[0].text(*at, row["source_id"], fontsize=8)
    for profile in plan["native_cut_context"]["profiles"]:
        xyz = np.array(profile["profile_xyz_m"])
        label = f"{profile['source_id']} @ {profile['source_station_m']} m"
        axes[0].plot(xyz[:, 0], xyz[:, 1], "k.-", linewidth=2)
        axes[1].plot(profile["offsets_m"], xyz[:, 2], ".-", label=label)
    axes[0].set(
        xlabel="east / m",
        ylabel="north / m",
        title="Bounded proposed domains; actual native frontiers",
    )
    axes[0].set_aspect("equal")
    axes[1].set(
        xlabel="transverse offset / m",
        ylabel="native height / m",
        title="Full native profiles including shoulders",
    )
    axes[1].legend(fontsize=8)
    figure.suptitle("Inputs and bindings only; replacement body remains unqualified")
    figure.tight_layout()
    figure.savefig(output / "native-cut-layout.png", dpi=160)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--trace-memory", action="store_true")
    args = parser.parse_args()
    if args.trace_memory:
        tracemalloc.start()
    plan, arrays, report = inspect_layout()
    if args.trace_memory:
        _current, report["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        report["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report["process_peak_rss_bytes"] = rss if sys.platform == "darwin" else rss * 1024
    report["instrumented"] = args.trace_memory
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "native-cut-plan.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(plan, args.output)
    print(
        json.dumps(
            {
                "contact_only": report["contact_only"]["valid"],
                "vertices": len(plan["xy"]),
                "scope": report["scope"],
            }
        )
    )


if __name__ == "__main__":
    main()
