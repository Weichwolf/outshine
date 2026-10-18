"""Actual whole SITE plants after the complete 105-source Transport/Buildings input."""

import argparse
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import stair_plant_sites as sites
from camera_height_inputs import digest
from stair_consumer_experiment import save
from stair_occupancy_plant_query import candidate_query, inputs
from stair_plant_prior import physical_form, previous
from stair_terrain_experiment import frozen


def pool_rows(pool):
    return [
        {
            "distance_m": r[0],
            "source_part": r[1],
            "site_id": list(r[2]),
            "root_xy_m": r[3].tolist(),
        }
        for r in pool
    ]


def generate(args, *, order=None, order_id="unchanged nearest order"):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh private /tmp whole-plant output required")
    if bool(args.prototype) != bool(args.prototype_receipt):
        raise ValueError("prototype and verification receipt are a pair")
    args.output.mkdir(parents=True)
    start = time.perf_counter()
    data, state, baseline, buildings = inputs(args.buildings, args.building_receipt)
    query, info = candidate_query(state, buildings)
    prior = previous(args.prototype, args.prototype_receipt) if args.prototype else None
    proto, artifacts, work = physical_form(args.output / "prototype", prior)
    evidence = {}
    old_proposals = sites.proposals

    def proposals(*positional, **named):
        pool, ledger = old_proposals(*positional, **named)
        before = pool_rows(pool)
        ordered = pool if order is None else order(pool, proto["bounds_m"])
        if (
            sorted(map(id, ordered)) != sorted(map(id, pool))
            or pool_rows(pool) != before
        ):
            raise ValueError(
                "proposal owner may only reorder the unchanged complete pool"
            )
        evidence.update(canonical_pool=before, ordered_pool=pool_rows(ordered))
        return ordered, ledger

    with patch.object(sites, "proposals", side_effect=proposals):
        accepted, ledger, budget = sites.plants(data, query, proto)
    budget["selection"] = (
        order_id + "; whole terrain/height/envelope/SITE checks unchanged"
    )
    artifacts.update(
        {
            "plants.json": save(args.output, "plants.json", accepted),
            "landcover-ledger.json": save(args.output, "landcover-ledger.json", ledger),
            "proposals.json": save(args.output, "proposals.json", evidence),
        }
    )
    report = {
        "commit": commit,
        "buildings": str(args.buildings.resolve()),
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "building_receipt": str(args.building_receipt.resolve()),
        "building_receipt_sha256": digest(args.building_receipt),
        "baseline": str(baseline.resolve()),
        "source_ids": state[4]["source_ids"],
        "frame": data["frame"].identifier,
        "actual_building_volumes": len(buildings),
        "original_landcover_parts": len(ledger),
        "accepted_plants": len(accepted),
        "prototype_vertices": proto["vertices"],
        "prototype_triangles": proto["triangles"],
        "prototype_array_bytes": work["logical_prototype_array_bytes"],
        "prototype_preparation": work,
        "prior_plants": None,
        "budget": budget,
        "order_id": order_id,
        "query_work": query.counts,
        "retained": info()._asdict(),
        "artifacts": artifacts,
        "seconds": time.perf_counter() - start,
        "scope": "Complete actual current Transport then Buildings then whole SITE plants; experiment query only, bounded attempts/form unchanged. XY distance/order is not free-space admission. No shared Plan or native admission.",
    }
    save(args.output, "comparison.json", report)
    print(json.dumps(report, indent=2))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--building-receipt", type=Path, required=True)
    parser.add_argument("--prototype", type=Path)
    parser.add_argument("--prototype-receipt", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    generate(parser.parse_args())
