"""Equal four fixed SITE results: global union versus bounded local support."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from recipe_plant_operators import SITE, prepared
from stair_consumer_inputs import inputs, source_rows
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_experiment import building_rows
from stair_plant_prior import previous
from stair_plant_retention import revalidate
from stair_plant_sites import candidate, terrain_domain
from stair_terrain_experiment import frozen


def evaluate(data, query, prior, local):
    rows = {r["id"]: r for r in source_rows(data, "landcover")[0]}
    graph = prepared(SITE)
    domain = None if local else terrain_domain(data)
    results, work = [], []
    for record in prior["records"]:
        row = rows[record["source_part_id"]]
        if local:
            value, cost = revalidate(
                data, query, prior["prototype"], row, record, candidate, graph
            )
            work.append(cost)
        else:
            old = record["result"]
            habitat = old["habitat"]
            canonical = (
                tuple(habitat["lower"]),
                habitat["side_m"],
                habitat["region_id"],
            )
            value = candidate(
                row,
                np.asarray(old["world_origin_m"][:2]),
                old["site_id"],
                row["polygon"].intersection(domain),
                data,
                query,
                prior["prototype"],
                canonical,
                graph,
            )
        results.append(value)
    return results, work


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private explicit /tmp output required")
    if args.output.exists():
        raise ValueError("fresh comparison output required")
    data = inputs(args.products)
    data["report_path"] = args.products / "comparison.json"
    buildings = building_rows(args.buildings, args.building_receipt, data)
    prior = previous(args.prior_plants, args.prior_receipt)
    query = OccupancyQuery((*data["reservations"], *buildings))
    observations, values = [], []
    for local in (False, True):
        tracemalloc.start()
        began = time.perf_counter()
        result, work = evaluate(data, query, prior, local)
        elapsed = time.perf_counter() - began
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        observations.append(
            {
                "method": "local" if local else "global",
                "seconds": elapsed,
                "python_traced_peak_bytes": peak,
                "accepted": len(result),
                "terrain_query_work": work,
            }
        )
        values.append(result)
    for cohort in values:
        for value in cohort:
            for key in ("root_support_query", "whole_body_query"):
                value[key].pop("seconds")
    if values[0] != values[1]:
        args.output.with_suffix(".differences.json").write_text(
            json.dumps(values, indent=2) + "\n"
        )
        raise ValueError("equal fixed SITE result comparison failed")
    report = {
        "commit": commit,
        "prior": prior["proof"],
        "observations": observations,
        "exact_four_SITE_results_except_query_seconds": True,
        "current_world_terrain_facets": len(data["terrain"].triangles),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Same fixed four sites, same cached form/query and loaded inputs. "
        "Selection includes source normalization and SITE graph compilation. "
        "Tracemalloc excludes untracked native allocations; RSS is process-wide. "
        "Global first/local second, loaded host; no device or form-emission speedup claim.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "products",
        "buildings",
        "building-receipt",
        "prior-plants",
        "prior-receipt",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
