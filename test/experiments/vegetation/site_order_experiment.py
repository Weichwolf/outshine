"""Same real SITE producer/reader and candidate pool with controlled query priority."""

import argparse
import hashlib
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import stair_plant_experiment as producer
import stair_plant_sites as sites
from site_order import envelope_radius, priority
from site_order_query import QueryCounter


def rows(pool):
    return [
        {
            "distance_m": row[0],
            "source_part": row[1],
            "site_id": list(row[2]),
            "root_xy_m": row[3].tolist(),
        }
        for row in pool
    ]


def fingerprint(value):
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def run(args):
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private explicit /tmp experiment output required")
    evidence = {"mode": args.order}
    original_proposals = sites.proposals

    def planting(data, query, proto, **kwargs):
        counter = QueryCounter(query)

        def proposals(*positional, **named):
            pool, ledger = original_proposals(*positional, **named)
            ordered = priority(pool, proto["bounds_m"], args.order)
            canonical = sorted(
                rows(pool), key=lambda r: (r["source_part"], r["site_id"])
            )
            evidence.update(
                pool_sha256=fingerprint(canonical),
                pool_count=len(pool),
                envelope_radius_m=envelope_radius(proto["bounds_m"]),
                ordered_proposals=rows(ordered),
            )
            return ordered, ledger

        with patch.object(sites, "proposals", side_effect=proposals):
            accepted, ledger, budget = sites.plants(data, counter, proto, **kwargs)
        if args.order == "boundary":
            budget["selection"] = (
                "same proposals: sufficient XY envelope radius first, ascending excess distance, then insufficient descending distance; stable source/site ties; full terrain/height/body/SITE checks unchanged"
            )
            budget["habitat_policy"] = (
                "unchanged supplied grass/wood and young Pinus; only root proposal processing order changed"
            )
        evidence.update(
            query_work=counter.counts,
            accepted=len(accepted),
            attempts=budget["attempts"],
            max_attempts=budget["max_attempts"],
            max_plants=budget["max_plants"],
        )
        return accepted, ledger, budget

    request = SimpleNamespace(
        products=args.products,
        buildings=args.buildings,
        building_receipt=args.building_receipt,
        output=args.output,
        prior_plants=None,
        prior_receipt=None,
    )
    with patch.object(producer, "plants", side_effect=planting):
        producer.run(request)
    path = args.output.with_name(args.output.name + "-order.json")
    path.write_text(json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--building-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--order", choices=("nearest", "boundary"), required=True)
    run(parser.parse_args())
