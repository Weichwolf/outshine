"""Existing real plant SITE recipe consumes actual transport/building clearance."""

import argparse
import json
import resource
import time
from pathlib import Path

import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import HeightInterval, Reservation
from stair_consumer_experiment import save
from stair_consumer_inputs import inputs
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_geometry import prototype
from stair_plant_sites import plants
from stair_terrain_experiment import frozen


def building_rows(products, receipt, data):
    report = json.loads((products / "comparison.json").read_text())
    verified = json.loads(receipt.read_text())
    if (
        verified["report_sha256"] != digest(products / "comparison.json")
        or not verified[
            "all_source_attributes_roots_heights_and_whole_body_conflicts_checked"
        ]
        or report["frame"] != data["frame"].identifier
        or report["products_report_sha256"]
        != digest(Path(report["products"]) / "comparison.json")
        or report["products_report_sha256"] != digest(Path(data["report_path"]))
    ):
        raise ValueError(
            "verified architecture in the identical transport frame required"
        )
    for name, expected in report["artifacts"].items():
        if digest(products / name) != expected:
            raise ValueError("actual building product changed: " + name)
    rows = json.loads((products / "building-reservations.json").read_text())
    if len(rows) != verified["actual_adjusted_bodies"]:
        raise ValueError("complete verified body reservations required")
    return tuple(
        Reservation(
            r["owner_id"],
            shapely.from_wkb(r["geometry_wkb"]),
            HeightInterval(*r["heights_m"]),
            r["purpose"],
        )
        for r in rows
    )


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private plant outputs require an explicit /tmp path")
    args.output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    data = inputs(args.products)
    data["report_path"] = args.products / "comparison.json"
    buildings = building_rows(args.buildings, args.building_receipt, data)
    loaded = time.perf_counter()
    mesh, proto = prototype(args.output / "prototype")
    built = time.perf_counter()
    combined = (*data["reservations"], *buildings)
    accepted, ledger, budget = plants(data, OccupancyQuery(combined), proto)
    resolved = time.perf_counter()
    artifacts = {
        "plants.json": save(args.output, "plants.json", accepted),
        "landcover-ledger.json": save(args.output, "landcover-ledger.json", ledger),
    }
    artifacts.update(
        {
            str(p.relative_to(args.output)): digest(p)
            for p in sorted((args.output / "prototype").iterdir())
        }
    )
    report = {
        "commit": commit,
        "products": str(args.products.resolve()),
        "transport_report_sha256": digest(args.products / "comparison.json"),
        "buildings": str(args.buildings.resolve()),
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "building_receipt": str(args.building_receipt.resolve()),
        "building_receipt_sha256": digest(args.building_receipt),
        "frame": data["frame"].identifier,
        "transport_volumes": len(data["reservations"]),
        "actual_building_volumes": len(buildings),
        "original_landcover_parts": len(ledger),
        "accepted_plants": len(accepted),
        "prototype_vertices": len(mesh.vertices),
        "prototype_triangles": len(mesh.triangles),
        "prototype_array_bytes": mesh.vertices.nbytes + mesh.triangles.nbytes,
        "budget": budget,
        "loading_seconds": loaded - began,
        "whole_form_seconds": built - loaded,
        "habitat_placement_seconds": resolved - built,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "artifacts": artifacts,
        "scope": "Actual existing young Pinus form and SITE placement; original wood habitats, actual current terrain, transport plus actual generated building reservations. Full measured AABB is conservative; no species observation, crown/crown packing, whole-terrain contact or native/world completion certificate.",
    }
    save(args.output, "comparison.json", report)
    print(json.dumps({k: v for k, v in report.items() if k != "artifacts"}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--building-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())
