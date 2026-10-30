"""Equal actual building queries: product bounds versus flat numeric facet scans."""

import argparse
import gc
import hashlib
import json
import os
import statistics
import time
from pathlib import Path
from unittest.mock import patch

import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Mesh
from process_memory import peak_rss_bytes
from rail_consumers_buildings import bound_products
from rail_occupancy_guard import RoundingGuardQuery, round_bounds
from rail_occupancy_inputs import catalog
from stair_occupancy_candidate_reuse import retained
from stair_occupancy_candidates import CandidateQuery
from stair_terrain_experiment import frozen


def cases(products):
    ledger = json.loads((products / "building-ledger.json").read_text())
    result = [
        (
            True,
            BuildingFootprint(
                row["id"],
                shapely.from_wkb(row["result"]["source_footprint_wkb"]),
                HeightInterval(*row["result"]["query_heights_m"]),
            ),
        )
        for row in ledger
        if row.get("state") == "prepared"
    ]
    result.extend(
        (
            False,
            BuildingFootprint(
                row["owner_id"],
                shapely.from_wkb(row["geometry_wkb"]),
                HeightInterval(*row["heights_m"]),
            ),
        )
        for row in json.loads((products / "building-reservations.json").read_text())
    )
    return result


def measured(query, guard, inputs):
    work, cuts = {}, []
    cpu, wall = time.process_time(), time.perf_counter()
    for planned, footprint in inputs:
        cut, note = (guard if planned else query).conflict(footprint)
        cuts.append(cut)
        for key in (
            "metadata_products",
            "metadata_face_tests",
            "metadata_raw_candidates",
            "metadata_materializer_calls",
        ):
            work[key] = work.get(key, 0) + note[key]
    cost = {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
    signature = hashlib.sha256(
        b"".join(shapely.normalize(c).wkb for c in cuts)
    ).hexdigest()
    return {**cost, "work": work, "cut_signature": signature}


def run(args):
    commit = frozen()
    report = json.loads((args.buildings / "comparison.json").read_text())
    receipt = json.loads(args.receipt.read_text())
    if receipt["report_sha256"] != digest(
        args.buildings / "comparison.json"
    ) or not receipt.get("float32_bodies_against_actual_raised_volumes"):
        raise ValueError("Same whole independently read building products required")
    for name, expected in report["artifacts"].items():
        if digest(args.buildings / name) != expected:
            raise ValueError("Actual cost input changed: " + name)
    rail, inputs = bound_products(report), cases(args.buildings)
    base, errors = catalog(rail), round_bounds(rail)
    operations, reuse = {}, {}
    for name in ("coarse", "flat"):
        prepared, info = retained(base, 8192)
        query = CandidateQuery(prepared, method=name)
        operations[name] = query, RoundingGuardQuery(query, errors)
        reuse[name] = info
    args.output.mkdir(parents=True, exist_ok=False)
    result = {
        "commit": commit,
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "queries": len(inputs),
        "product_metadata_count": len(base.sources),
        "facet_metadata_count": sum(len(s.bounds) for s in base.sources),
        "held_numeric_bounds_bytes": sum(s.bounds.nbytes for s in base.sources),
        "held_envelope_bytes": sum(e.nbytes for e in base.envelopes),
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "methods": {},
    }
    rounds = {name: [] for name in operations}
    with patch.object(
        Mesh,
        "__post_init__",
        side_effect=AssertionError("Query cannot clone body meshes"),
    ):
        warm = {
            name: measured(*operation, inputs) for name, operation in operations.items()
        }
        signature = warm["coarse"]["cut_signature"]
        if warm["flat"]["cut_signature"] != signature:
            raise ValueError("Actual complete cost-control cuts differ")
        for pair in range(args.pairs):
            names = list(operations)
            if pair % 2:
                names.reverse()
            for name in names:
                gc.collect()
                note = measured(*operations[name], inputs)
                if note["cut_signature"] != signature:
                    raise ValueError("Measured complete geometry changed")
                rounds[name].append(note)
        for name in operations:
            result["methods"][name] = {
                "rounds": rounds[name],
                "median_cpu_s": statistics.median(n["cpu_s"] for n in rounds[name]),
                "median_wall_s": statistics.median(n["wall_s"] for n in rounds[name]),
                "retained": reuse[name]()._asdict(),
            }
        if args.profile:
            import memray

            for name, operation in operations.items():
                path = args.output / (name + "-allocations.bin")
                gc.collect()
                with memray.Tracker(str(path), trace_python_allocators=True):
                    profiled = measured(*operation, inputs)
                if profiled["cut_signature"] != signature:
                    raise ValueError("Profiled quality changed")
                reader = memray.FileReader(path)
                events = requested = 0
                for record in reader.get_allocation_records():
                    if record.size > 0:
                        events += record.n_allocations
                        requested += record.size
                result["methods"][name]["profile"] = {
                    "allocation_events": events,
                    "requested_allocation_bytes": requested,
                    "tracked_peak_heap_bytes": reader.metadata.peak_memory,
                    "trace_sha256": digest(path),
                    "work": profiled["work"],
                }
    result.update(
        process_peak_rss_bytes=peak_rss_bytes(),
        all_actual_cut_signatures_equal=True,
        mesh_constructor_calls=0,
        scope="Same whole source-height planning requests and actual saved body queries. Warm bounded facet cache; alternating normal-priority pairs. Preparation/decode, construction/CGA, serialization and rendering excluded. Profile includes signature bookkeeping; requested bytes are not copied/live/RSS. No Mesh constructors in query paths; NumPy/GEOS total copying unknown. Python only, no native/device/world/p99/UMA claim.",
    )
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "queries": len(inputs),
                "methods": {
                    name: {k: note[k] for k in ("median_cpu_s", "median_wall_s")}
                    for name, note in result["methods"].items()
                },
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 8), default=3)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp cost output required")
    run(args)
