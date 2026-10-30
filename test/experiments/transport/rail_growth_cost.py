"""Compare a whole ground replay with appending to its already held raised base."""

import argparse
import gc
import json
import os
import statistics
from pathlib import Path

from camera_height_inputs import digest
from rail_consumers_buildings import bound_products
from rail_occupancy_growth import extend
from rail_occupancy_inputs import load
from rail_occupancy_state_cost import measured, signature
from stair_local_replay import load_chain
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    report = json.loads((args.buildings / "comparison.json").read_text())
    receipt = json.loads(args.receipt.read_text())
    if receipt.get("report_sha256") != digest(
        args.buildings / "comparison.json"
    ) or not receipt.get("float32_bodies_against_actual_raised_volumes"):
        raise ValueError("Independently read whole Buildings required")
    expected = signature(bound_products(report).state)
    base = load(Path(report["elevated_supports"]))
    resident, prior = base.state_inputs
    extensions = tuple(Path(row["path"]) for row in report["transport_extensions"])
    operations = {
        "full_ground_replay": lambda: load_chain(resident, (*prior, *extensions)),
        "append_to_held_base": lambda: extend(base, extensions)[0].state,
    }
    args.output.mkdir(parents=True, exist_ok=False)
    result = {
        "commit": commit,
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "building_receipt_sha256": digest(args.receipt),
        "complete_state_signature": expected,
        "held_ground_sources": len(base.state[4]["source_ids"]),
        "held_road_assets": len(base.state[1]),
        "extension_count": len(extensions),
        "held_raised_supports": len(base.supports),
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "methods": {name: {"rounds": []} for name in operations},
    }

    def check(state, name):
        if signature(state) != expected:
            raise ValueError("Complete replay/append state differs")
        if name == "append_to_held_base" and any(
            a is not b for a, b in zip(base.state[1], state[1], strict=False)
        ):
            raise ValueError("Every held old road asset must retain identity")

    for pair in range(args.pairs):
        order = tuple(operations) if pair % 2 == 0 else tuple(operations)[::-1]
        for name in order:
            gc.collect()
            state, note = measured(operations[name])
            check(state, name)
            result["methods"][name]["rounds"].append(note)
            del state
    for name, operation in operations.items():
        note = result["methods"][name]
        for kind in ("cpu_s", "wall_s"):
            note["median_" + kind] = statistics.median(
                row[kind] for row in note["rounds"]
            )
        if args.profile:
            import memray

            path = args.output / (name + ".bin")
            gc.collect()
            with memray.Tracker(str(path), trace_python_allocators=True):
                state, profiled = measured(operation)
            check(state, name)
            del state
            reader = memray.FileReader(path)
            events = requested = 0
            for record in reader.get_allocation_records():
                if record.size > 0:
                    events += record.n_allocations
                    requested += record.size
            note["profile"] = {
                "allocation_events": events,
                "requested_allocation_bytes": requested,
                "tracked_peak_heap_bytes": reader.metadata.peak_memory,
                "trace_sha256": digest(path),
                "work": profiled["work"],
            }
    result["scope"] = (
        "Same independently read complete ground result, including terrain masks, "
        "road arrays, supports, contacts and manifests. Both start with the same "
        "already held qualified raised/base products. Initial loading, generation, "
        "Buildings/SITE and signature work excluded. Full replay is only the "
        "redundant ground reconstruction control; append additionally checks "
        "whole changed-region/body separation from held raised faces. Mesh copies "
        "are an explicit subset; total decoder/NumPy/GEOS copies unknown. Scoped "
        "allocation peak excludes held inputs and is not process/UMA/device peak. "
        "Python only; no native/frame/p99/World claim."
    )
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({n: r["median_cpu_s"] for n, r in result["methods"].items()}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 8), default=5)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp output required")
    run(args)
