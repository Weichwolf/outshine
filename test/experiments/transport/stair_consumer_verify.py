"""Fresh saved-body reader rechecks all current roots and full transport conflicts."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_consumer_buildings import body_query
from stair_consumer_inputs import inputs, root_contact, source_rows
from stair_occupancy_handoff import OccupancyQuery
from stair_terrain_experiment import frozen


def meshes(path):
    with np.load(path, allow_pickle=False) as saved:
        count = len(saved.files) // 2
        if set(saved.files) != {
            f"{i}_{key}" for i in range(count) for key in ("vertices", "faces")
        }:
            raise ValueError("saved body arrays missing or duplicated")
        return [Mesh(saved[f"{i}_vertices"], saved[f"{i}_faces"]) for i in range(count)]


def run(args):
    commit = frozen()
    output = args.output
    if not output.resolve().is_relative_to(Path("/tmp").resolve()) or output.exists():
        raise ValueError("fresh explicit /tmp verification receipt required")
    report = json.loads((args.products / "comparison.json").read_text())
    for name, expected in report["artifacts"].items():
        if digest(args.products / name) != expected:
            raise ValueError("saved actual consumer artifact changed: " + name)
    data = inputs(Path(report["products"]))
    originals, rejected = source_rows(data, "building")
    by_id = {r["id"]: r for r in originals}
    ledger = json.loads((args.products / "building-ledger.json").read_text())
    if len(ledger) != len(originals) + len(rejected):
        raise ValueError("source-part ledger incomplete")
    if [r["source_id"] for r in ledger[: len(rejected)]] != [
        r["source_id"] for r in rejected
    ] or [r["id"] for r in ledger[len(rejected) :]] != list(by_id):
        raise ValueError("original source/part identities changed")
    before = meshes(args.products / "buildings-before.npz")
    after = meshes(args.products / "buildings-after.npz")
    query = OccupancyQuery(data["reservations"])
    i, j = 0, 0
    for row in ledger[len(rejected) :]:
        original = by_id[row["id"]]
        if row["original_feature"] != original["original_feature"]:
            raise ValueError("original provider geometry/attributes lost")
        if row["state"] == "deferred":
            if not row.get("reason"):
                raise ValueError("unresolved original requires an explicit reason")
            continue
        result = row["result"]
        contact = root_contact(data, original["polygon"])
        if contact != result["contact"]:
            raise ValueError("current actual terrain contact changed")
        _, requested = body_query(row["id"], before[i], 0)
        i += 1
        if [requested.heights.lower_m, requested.heights.upper_m] != result[
            "query_heights_m"
        ]:
            raise ValueError("query band does not bound actual original recipe")
        for part in result["products"]:
            _, body = body_query(row["id"], after[j], 0)
            j += 1
            if [body.heights.lower_m, body.heights.upper_m] != part["body_heights_m"]:
                raise ValueError("saved actual body heights changed")
            conflict, _ = query.conflict(body, method="scan")
            if not conflict.is_empty:
                raise ValueError("saved actual architecture conflicts with transport")
    if i != len(before) or j != len(after):
        raise ValueError("body ownership incomplete or duplicated")
    result = {
        "reader_commit": commit,
        "producer_commit": report["commit"],
        "report_sha256": digest(args.products / "comparison.json"),
        "original_source_parts": len(ledger),
        "original_architecture_bodies": i,
        "actual_adjusted_bodies": j,
        "all_source_attributes_roots_heights_and_whole_body_conflicts_checked": True,
        "scope": "Fresh saved-body full scan on original inputs/current terrain. Independent installed producer replay establishes recipe equivalence separately; no general nonempty GEOS topology/foundation/native certificate.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
