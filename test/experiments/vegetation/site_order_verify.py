"""Independent real proposal replay, unchanged whole-form comparison and prefix audit."""

import argparse
import copy
import json
import math
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_consumer_inputs import inputs, source_rows
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_experiment import building_rows
from stair_plant_sites import proposals, region, terrain_domain


def read(products, receipt):
    report = json.loads((products / "comparison.json").read_text())
    proof = json.loads(receipt.read_text())
    if (
        proof["report_sha256"] != digest(products / "comparison.json")
        or proof["producer_commit"] != report["commit"]
        or not proof["whole_form_replayed_exact"]
        or not proof["all_physical_bounds_and_SITE_products_verified"]
    ):
        raise ValueError("same-product complete physical reader required")
    for name, expected in report["artifacts"].items():
        if digest(products / name) != expected:
            raise ValueError("verified physical product changed: " + name)
    return report, json.loads((products / "plants.json").read_text())


def descriptor(row):
    return row[1], tuple(row[2]), tuple(row[3]), row[0]


def semantic_plants(records):
    result = copy.deepcopy(records)
    for row in result:
        for key in ("root_support_query", "whole_body_query"):
            row["result"][key].pop("seconds")
    return result


def check_prefix(products, records, report, evidence, expected):
    actual_order = [
        (r["source_part"], tuple(r["site_id"]), tuple(r["root_xy_m"]), r["distance_m"])
        for r in evidence["ordered_proposals"]
    ]
    if actual_order != list(map(descriptor, expected)):
        raise ValueError("proposal sequence differs from independent ordering")
    attempts = report["budget"]["attempts"]
    prefix = actual_order[:attempts]
    positions = {(row[0], row[1]): i for i, row in enumerate(prefix)}
    ledger = json.loads((products / "landcover-ledger.json").read_text())
    visited, accepted = [], []
    for entry in ledger:
        indices = []
        for site in entry.get("sites", []):
            key = entry["id"], tuple(site["site_id"])
            if key not in positions:
                raise ValueError(
                    "attempted an unrequested site outside the priority prefix"
                )
            index = positions[key]
            if tuple(site["root_xy_m"]) != prefix[index][2]:
                raise ValueError("attempted root was not an unchanged proposal")
            indices.append(index)
            visited.append(key)
            if site["state"] == "prepared":
                accepted.append((site["plant_index"], key, index))
        if indices != sorted(indices):
            raise ValueError("within-source attempted priority changed")
    if (
        len(visited) != attempts
        or len(set(visited)) != attempts
        or set(visited) != set(positions)
    ):
        raise ValueError("attempt ledger is not the complete bounded priority prefix")
    accepted.sort()
    keys = [(r["source_part_id"], tuple(r["result"]["site_id"])) for r in records]
    if keys != [r[1] for r in accepted] or [r[2] for r in accepted] != sorted(
        r[2] for r in accepted
    ):
        raise ValueError("admitted plants do not follow the verified attempted prefix")


def compare(reference, reference_receipt, trial, trial_receipt, output):
    reports, records = zip(
        read(reference, reference_receipt), read(trial, trial_receipt), strict=True
    )
    first, second = reports
    for key in (
        "transport_report_sha256",
        "building_report_sha256",
        "building_receipt_sha256",
    ):
        if first[key] != second[key]:
            raise ValueError("comparison changed the full input world")
    proto = json.loads((reference / "prototype/prototype.json").read_text())
    if proto != json.loads((trial / "prototype/prototype.json").read_text()):
        raise ValueError("complete prototype metadata/array digests changed")
    for filename in proto["artifacts"]:
        with (
            np.load(reference / "prototype" / filename, allow_pickle=False) as a,
            np.load(trial / "prototype" / filename, allow_pickle=False) as b,
        ):
            if a.files != b.files:
                raise ValueError("prototype array schema changed")
            for name in a.files:
                np.testing.assert_array_equal(a[name], b[name])
    data = inputs(Path(first["products"]))
    data["report_path"] = Path(first["products"]) / "comparison.json"
    buildings = building_rows(
        Path(first["buildings"]), Path(first["building_receipt"]), data
    )
    query = OccupancyQuery((*data["reservations"], *buildings))
    rows, _ = source_rows(data, "landcover")
    pool, _ = proposals(data, proto, terrain_domain(data), region(data), rows, query)
    radius = math.nextafter(
        math.hypot(*np.max(abs(np.asarray(proto["bounds_m"])[:, :2]), axis=0)), math.inf
    )
    ordered = sorted(
        pool, key=lambda r: (r[0] < radius, abs(r[0] - radius), r[1], r[2])
    )
    for products, values, report, expected in zip(
        (reference, trial), records, reports, (pool, ordered), strict=True
    ):
        evidence = json.loads(
            products.with_name(products.name + "-order.json").read_text()
        )
        check_prefix(products, values, report, evidence, expected)
    for key in ("max_attempts", "max_plants", "max_proposals"):
        if first["budget"][key] != second["budget"][key]:
            raise ValueError("comparison changed demand/attempt capacity")
    if len(records[1]) != second["budget"]["max_plants"]:
        raise ValueError("this positive control did not fill unchanged plant demand")
    result = {
        "same_full_world_and_prototype": True,
        "same_actual_proposal_set": True,
        "complete_priority_prefixes_checked": True,
        "proposals": len(pool),
        "reference_plants": len(records[0]),
        "trial_plants": len(records[1]),
        "physical_logical_building_volumes": len(query.reservations),
        "scope": "Actual positive control; full original readers bind shape/SITE legality. No global maximal planting or distribution claim.",
    }
    if not output.resolve().is_relative_to(Path("/tmp")) or output.exists():
        raise ValueError("fresh explicit /tmp comparison output required")
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("trial", type=Path)
    parser.add_argument("--reference-receipt", type=Path, required=True)
    parser.add_argument("--trial-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(
        args.reference,
        args.reference_receipt,
        args.trial,
        args.trial_receipt,
        args.output,
    )
