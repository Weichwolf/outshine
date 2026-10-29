"""Compare early demand rejection with the same full seed selection filtered late."""

import argparse
import gc
import json
import os
import platform
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from allocation_measure import allocations
from camera_height_inputs import digest
from stair_endpoint_frontier_study import query_context
from stair_frontier_sources import records_for
from stair_module_inputs import reference_profiles
from stair_patch_inputs import fresh
from stair_seed_axes import axis_form
from stair_seed_sources import select, separated_records
from stair_terrain_experiment import frozen


def eligible(records):
    return [r for r in records if r["eligible"]]


def late_selection(context, bounds):
    rows, work = separated_records(
        context.view, context.assets, context.manifest, context.sources
    )
    profiles, requested, builds = reference_profiles(), shapely.box(*bounds), 0
    for row in rows:
        if not row["eligible"]:
            continue
        source = context.sources[row["source_id"]]
        kind = source["properties"].get("subclass") or "unclassified"
        band = source["shape"].buffer(
            profiles[kind]["widthM"] / 2,
            cap_style="flat",
            join_style="mitre",
            mitre_limit=2,
        )
        builds += 1
        if not requested.intersects(band):
            row.update(eligible=False, reason="outside_requested_source_bounds")
    return rows, dict(work, late_request_corridor_builds=builds)


def proposal_witness(context, bounds, result):
    with patch("stair_seed_sources.separated_records", return_value=result):
        saved = select(
            context.view,
            context.assets,
            context.manifest,
            context.sources,
            context.reference,
            requested_bounds=bounds,
        )
    return {key: saved[key] for key in ("roots", "recipes", "eligible_sources")}


def measure(calls, expected, output):
    costs = {name: {"rounds": []} for name in calls}
    names = tuple(calls)
    for turn in range(9):
        order = names if turn % 2 == 0 else names[::-1]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            rows, _work = calls[name]()
            costs[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if eligible(rows) != expected:
                raise ValueError("Repeated complete requested source selection changed")
            del rows
    for name, call in calls.items():
        gc.collect()
        (rows, work), memory = allocations(output / (name + ".bin"), call)
        if eligible(rows) != expected:
            raise ValueError(
                "Allocation run changed complete requested source selection"
            )
        costs[name].update(work=work, allocations=memory)
        for clock in ("cpu_s", "wall_s"):
            values = [r[clock] for r in costs[name]["rounds"]]
            costs[name][clock] = {
                f"p{p}": float(np.percentile(values, p)) for p in (50, 95)
            }
    return costs


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal priority")
    receipt = json.loads((args.study / "report.json").read_text())
    saved = json.loads((args.study / "selection.json").read_text())["index"]
    if (
        not receipt["valid_automatic_module"]
        or receipt.get("selection_family") != "seed"
        or receipt["selection_sha256"] != digest(args.study / "selection.json")
        or receipt["query_sha256"] != digest(args.query)
    ):
        raise ValueError(
            "Actual independently read seed product for this query required"
        )
    context = query_context(args)
    if context.manifest["source_ids"] != saved["before_source_ids"]:
        raise ValueError("Actual preceding whole source ownership changed")
    xy = context.state["plan"]["xy"]
    bounds = np.r_[xy.min(axis=0), xy.max(axis=0)]
    if bounds.tolist() != saved["requested_bounds_xy_m"]:
        raise ValueError("Actual conservative requested bounds changed")
    calls = {
        "early": lambda: separated_records(
            context.view,
            context.assets,
            context.manifest,
            context.sources,
            requested_bounds=bounds,
        ),
        "late": lambda: late_selection(context, bounds),
    }
    results = {name: call() for name, call in calls.items()}
    wanted = eligible(results["early"][0])
    witnesses = [proposal_witness(context, bounds, r) for r in results.values()]
    expected = {key: saved[key] for key in witnesses[0]}
    if not wanted or any(w != expected for w in witnesses):
        raise ValueError("Whole requested roots, recipes or source profiles changed")
    if eligible(results["late"][0]) != wanted:
        raise ValueError("Late whole-source request filtering changed eligibility")
    domain = np.array([p.bounds_xy_m for p in context.view.patches]).reshape(-1, 2)
    preflight_work = {}
    preflight = records_for(
        {
            "plan": {"xy": domain},
            "report": {"candidate_source_ids": context.manifest["source_ids"]},
        },
        context.sources,
        work=preflight_work,
    )
    probe_bands = 0
    for row in eligible(preflight):
        try:
            axis_form(context.sources[row["source_id"]])
        except ValueError:
            continue
        probe_bands += 1
    load = os.getloadavg()
    args.output.mkdir()
    costs = measure(calls, wanted, args.output)
    costs["late"]["work"].update(
        source_selection=preflight_work, old_probe_corridor_builds=probe_bands
    )
    copied_axes = [
        context.sources[r["source_id"]]["xy_m"]
        for r in preflight
        if r["reason"]
        in (
            "",
            "outside_current_complete_terrain",
            "whole_axis_geometry_budget_or_degeneracy",
        )
    ]
    report = {
        "commit": commit,
        "query_sha256": digest(args.query),
        "study_report_sha256": digest(args.study / "report.json"),
        "resident_sha256": saved["resident_sha256"],
        "parent_reports": saved["parent_reports"],
        "requested_bounds_xy_m": bounds.tolist(),
        "same_eligible_whole_sources": [r["source_id"] for r in wanted],
        "same_roots_recipes_and_profiles": True,
        "recipes": len(expected["recipes"]),
        "costs": costs,
        "known_source_xy_copies_per_call": len(copied_axes),
        "known_source_xy_copied_bytes_per_call": sum(
            np.asarray(xy, float).nbytes for xy in copied_axes
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "loadavg_before": load,
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "scope": "Already held immutable sources/assets; candidate eligibility only. Identical requested whole sources, roots and physical recipes; all-old-body checks retained. Global source loading, graph discovery, generation, consumers, GPU and device p99 excluded. Outside-request rejection priority may differ. Both paths still copy source axes during semantic/degeneracy checks; only those known copies are enumerated. Memray requested bytes are not copied or live bytes; total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"commit": commit, "output": str(args.output), "eligible": len(wanted)}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "query", "study", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())
