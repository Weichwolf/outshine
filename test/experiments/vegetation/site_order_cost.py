"""Search-only allocation evidence on the same verified full real consumer inputs."""

import argparse
import json
from pathlib import Path
from unittest.mock import patch

import memray
import stair_plant_sites as sites
from site_order import priority
from site_order_query import QueryCounter
from site_order_verify import read, semantic_plants
from stair_consumer_inputs import inputs
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_experiment import building_rows


def run(products, receipt, output, order):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private profile output must be under /tmp")
    report, expected = read(products, receipt)
    data = inputs(Path(report["products"]))
    data["report_path"] = Path(report["products"]) / "comparison.json"
    buildings = building_rows(
        Path(report["buildings"]), Path(report["building_receipt"]), data
    )
    proto = json.loads((products / "prototype/prototype.json").read_text())
    query = QueryCounter(OccupancyQuery((*data["reservations"], *buildings)))
    original = sites.proposals

    def proposals(*args, **kwargs):
        pool, ledger = original(*args, **kwargs)
        return priority(pool, proto["bounds_m"], order), ledger

    output.mkdir(parents=True, exist_ok=False)
    with (
        memray.Tracker(output / "allocations.bin", trace_python_allocators=True),
        patch.object(sites, "proposals", side_effect=proposals),
    ):
        accepted, _, budget = sites.plants(
            data,
            query,
            proto,
            max_plants=report["budget"]["max_plants"],
            max_attempts=report["budget"]["max_attempts"],
        )
    if (
        semantic_plants(accepted) != semantic_plants(expected)
        or budget["attempts"] != report["budget"]["attempts"]
    ):
        raise ValueError("instrumented search changed the actual admitted SITE product")
    result = {
        "order": order,
        "accepted": len(accepted),
        "attempts": budget["attempts"],
        "query_work": query.counts,
        "tracked_peak_bytes": memray.FileReader(
            output / "allocations.bin"
        ).metadata.peak_memory,
        "scope": "Same current data/prototype preloaded. Search includes original proposals, changed priority and full checks; excludes IO/prototype geometry generation and order-evidence serialization. Different admitted outputs; no equal-output runtime speedup or device claim.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--order", choices=("nearest", "boundary"), required=True)
    args = parser.parse_args()
    run(args.products, args.receipt, args.output, args.order)
