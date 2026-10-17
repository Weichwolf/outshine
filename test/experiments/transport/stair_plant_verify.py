"""Fresh whole-form/SITE replay and full-scan physical bounds for saved plants."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from recipe_plant_operators import SITE, prepared
from stair_consumer_inputs import inputs, source_rows
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_experiment import building_rows
from stair_plant_geometry import bounds_query, prototype
from stair_plant_sites import candidate, region, terrain_domain
from stair_terrain_experiment import frozen


def replay_prototype(products):
    saved = json.loads((products / "prototype/prototype.json").read_text())
    with np.load(products / "prototype/complete.npz", allow_pickle=False) as data:
        mesh = Mesh(data["vertices"], data["faces"])
        leaf = data["leaf_faces"]
    with tempfile.TemporaryDirectory(
        prefix="/tmp/outshine-transport-plant-reader-"
    ) as temporary:
        expected, report = prototype(Path(temporary) / "reference")
        np.testing.assert_array_equal(mesh.vertices, expected.vertices)
        np.testing.assert_array_equal(mesh.triangles, expected.triangles)
        with np.load(
            Path(temporary) / "reference/complete.npz", allow_pickle=False
        ) as data:
            np.testing.assert_array_equal(leaf, data["leaf_faces"])
        if report != saved:
            raise ValueError(
                "actual recipe, whole-form parts/poses or prototype bytes changed"
            )
    return mesh, saved


def replay_plants(products, data, query, mesh, proto):
    rows, rejected = source_rows(data, "landcover")
    ledger = json.loads((products / "landcover-ledger.json").read_text())
    accepted = json.loads((products / "plants.json").read_text())
    if ledger[: len(rejected)] != rejected or len(ledger) != len(rows) + len(rejected):
        raise ValueError("complete original habitat ledger required")
    by_id = {r["id"]: r for r in rows}
    if [r["id"] for r in ledger[len(rejected) :]] != list(by_id):
        raise ValueError("original habitat part order/identity changed")
    owners = []
    for entry in ledger[len(rejected) :]:
        original = by_id[entry["id"]]
        if any(entry[k] != v for k, v in original.items() if k != "polygon"):
            raise ValueError("original habitat geometry or attributes lost")
        for site in entry["sites"]:
            if site["state"] == "prepared":
                owners.append(site["plant_index"])
            elif not site.get("reason"):
                raise ValueError("local unresolved root requires its reason")
    if sorted(owners) != list(range(len(accepted))):
        raise ValueError("actual plant owners incomplete or duplicated")
    domain = terrain_domain(data)
    lower, side, region_id = region(data)
    graph = prepared(SITE)
    for record in accepted:
        row = by_id[record["source_part_id"]]
        result = record["result"]
        origin = np.array(result["world_origin_m"])
        xyz = mesh.vertices + origin
        body = bounds_query(row["id"], [xyz.min(axis=0), xyz.max(axis=0)], [0, 0, 0])
        if [body.heights.lower_m, body.heights.upper_m] != result[
            "query_heights_m"
        ] or body.geometry.wkb_hex != result["query_geometry_wkb"]:
            raise ValueError("saved bounds differ from actual entire transformed plant")
        if not query.conflict(body, method="scan")[0].is_empty:
            raise ValueError("whole physical plant enters actual transport/building")
        expected = candidate(
            row,
            origin[:2],
            result["site_id"],
            row["polygon"].intersection(domain),
            data,
            query,
            proto,
            (lower, side, region_id + ":" + row["id"]),
            graph,
        )
        for key in result.keys() - {"root_support_query", "whole_body_query"}:
            value = json.loads(json.dumps(expected[key]))
            if value != result[key]:
                raise ValueError("actual root/habitat/SITE product changed: " + key)
    return len(ledger), len(accepted)


def run(args):
    commit = frozen()
    if (
        not args.output.resolve().is_relative_to(Path("/tmp").resolve())
        or args.output.exists()
    ):
        raise ValueError("fresh explicit /tmp plant receipt required")
    report = json.loads((args.products / "comparison.json").read_text())
    for name, expected in report["artifacts"].items():
        if digest(args.products / name) != expected:
            raise ValueError("complete saved plant/habitat artifact changed: " + name)
    data = inputs(Path(report["products"]))
    data["report_path"] = Path(report["products"]) / "comparison.json"
    buildings = building_rows(
        Path(report["buildings"]), Path(report["building_receipt"]), data
    )
    if (
        digest(Path(report["buildings"]) / "comparison.json")
        != report["building_report_sha256"]
    ):
        raise ValueError("actual building products changed")
    mesh, proto = replay_prototype(args.products)
    count, accepted = replay_plants(
        args.products,
        data,
        OccupancyQuery((*data["reservations"], *buildings)),
        mesh,
        proto,
    )
    receipt = {
        "reader_commit": commit,
        "producer_commit": report["commit"],
        "report_sha256": digest(args.products / "comparison.json"),
        "original_habitat_parts": count,
        "actual_complete_plants": accepted,
        "whole_form_replayed_exact": True,
        "all_physical_bounds_and_SITE_products_verified": True,
        "scope": "Full actual prototype re-emission, original habitats, current root contacts, real SITE replay and all transport/building volumes by scan. No general binary64 nonempty-topology or full crown/terrain certificate.",
    }
    args.output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
