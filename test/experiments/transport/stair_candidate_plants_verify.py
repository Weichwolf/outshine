"""Independent full-volume reader of current whole plants and proposal membership."""

import argparse
import json
import resource
from pathlib import Path

from camera_height_inputs import digest
from stair_candidate_plants import pool_rows
from stair_consumer_inputs import source_rows
from stair_occupancy_plant_query import inputs, reference_query
from stair_plant_sites import proposals, region, terrain_domain
from stair_plant_verify import replay_plants, replay_prototype
from stair_terrain_experiment import frozen


def attempted_prefix(products, report, canonical):
    saved = json.loads((products / "proposals.json").read_text())
    if saved["canonical_pool"] != pool_rows(canonical):
        raise ValueError("complete independent proposal membership/geometry changed")
    original = {json.dumps(r, sort_keys=True) for r in saved["canonical_pool"]}
    ordered = [json.dumps(r, sort_keys=True) for r in saved["ordered_pool"]]
    if len(ordered) != len(original) or set(ordered) != original:
        raise ValueError(
            "proposal order removed, duplicated or changed a whole proposal"
        )
    attempts = report["budget"]["attempts"]
    if report["budget"]["max_attempts"] != 128 or report["budget"]["max_plants"] != 4:
        raise ValueError("unchanged plant/attempt control limits required")
    if not 0 <= attempts <= 128 or report["accepted_plants"] > 4:
        raise ValueError("whole plant selection exceeded its explicit control budget")
    ledger = json.loads((products / "landcover-ledger.json").read_text())
    entries = [
        ((row["id"], tuple(site["site_id"])), site)
        for row in ledger
        if "id" in row
        for site in row["sites"]
    ]
    attempted = dict(entries)
    if len(entries) != len(attempted):
        raise ValueError("duplicate actual root attempt identity")
    if (
        report["budget"]["max_proposals"] != 1024
        or report["budget"]["proposals"] != len(ordered)
        or len(ordered) > 1024
    ):
        raise ValueError("unchanged whole proposal budget required")
    expected = saved["ordered_pool"][:attempts]
    keys = [(r["source_part"], tuple(r["site_id"])) for r in expected]
    if set(keys) != set(attempted) or len(keys) != len(attempted):
        raise ValueError(
            "actual attempts are not the complete declared proposal prefix"
        )
    prepared = 0
    for entry, key in zip(expected, keys, strict=True):
        actual = attempted[key]
        if (
            actual["root_xy_m"] != entry["root_xy_m"]
            or actual["nearest_volume_xy_distance_m"] != entry["distance_m"]
        ):
            raise ValueError("actual root attempt changed its original proposal")
        if actual["state"] == "prepared":
            if actual["plant_index"] != prepared:
                raise ValueError("actual whole plant order changed")
            prepared += 1
    if prepared != report["accepted_plants"]:
        raise ValueError("whole plant ownership/count differs from actual attempts")
    return len(ordered), attempts


def verify(products, output):
    commit = frozen()
    if not output.resolve().is_relative_to(Path("/tmp")) or output.exists():
        raise ValueError("fresh private /tmp whole-plant receipt required")
    report = json.loads((products / "comparison.json").read_text())
    for name, expected in report["artifacts"].items():
        path = products / name
        if (
            not path.resolve().is_relative_to(products.resolve())
            or digest(path) != expected
        ):
            raise ValueError("complete actual plant artifact changed: " + name)
    buildings, receipt = Path(report["buildings"]), Path(report["building_receipt"])
    if (
        digest(buildings / "comparison.json") != report["building_report_sha256"]
        or digest(receipt) != report["building_receipt_sha256"]
    ):
        raise ValueError("identical verified whole building input required")
    data, state, baseline, bodies = inputs(buildings, receipt)
    if (
        report["frame"] != data["frame"].identifier
        or report["source_ids"] != state[4]["source_ids"]
    ):
        raise ValueError("whole current frame/source ownership changed")
    query = reference_query(baseline, state, bodies)
    mesh, proto = replay_prototype(products)
    if (
        report["prototype_vertices"] != len(mesh.vertices)
        or report["prototype_triangles"] != len(mesh.triangles)
        or report["prototype_array_bytes"]
        != mesh.vertices.nbytes + mesh.triangles.nbytes
    ):
        raise ValueError("whole real prototype arrays differ from their dimensions")
    rows, _rejected = source_rows(data, "landcover")
    canonical, _ledger = proposals(
        data, proto, terrain_domain(data), region(data), rows, query
    )
    proposal_count, attempts = attempted_prefix(products, report, canonical)
    count, accepted = replay_plants(products, data, query, mesh, proto, report)
    if (
        accepted != report["accepted_plants"]
        or len(bodies) != report["actual_building_volumes"]
    ):
        raise ValueError("whole actual consumer count changed")
    result = {
        "reader_commit": commit,
        "producer_commit": report["commit"],
        "report_sha256": digest(products / "comparison.json"),
        "transport_sources": len(state[4]["source_ids"]),
        "actual_building_volumes": len(bodies),
        "original_habitat_parts": count,
        "actual_complete_plants": accepted,
        "proposal_count": proposal_count,
        "attempts": attempts,
        "whole_form_replayed_exact": True,
        "all_physical_bounds_and_SITE_products_verified": True,
        "complete_proposals_and_attempt_prefix_checked": True,
        "scope": "Fresh whole form/SITE replay, complete original source proposals and exact distances, unchanged physical/logical transport partitions and all actual building parts by full scan. Caller owns proposal priority; permutation/prefix verified, not ecological distribution or native admission.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    args = parser.parse_args()
    verify(args.products, args.output)
