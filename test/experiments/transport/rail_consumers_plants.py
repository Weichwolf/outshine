"""Whole existing SITE recipes consume saved raised transport and actual Buildings."""

import argparse
import json
import resource
import tempfile
from functools import partial
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Reservation
from rail_consumers_buildings import bound_products
from rail_occupancy_inputs import supplements
from rail_occupancy_reference import RailReference, whole_raised
from stair_candidate_plants import generate
from stair_candidate_plants_verify import verify as verify_plants
from stair_consumer_buildings import body_query
from stair_consumer_experiment import save
from stair_consumer_verify import meshes
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_plant_query import (
    PlantQuery,
    ReferenceDistance,
    candidate_query,
    inputs,
)
from stair_plant_geometry import bounds_query
from stair_support_audit import quantized


def adapter():
    held = {}

    def prepare(products, receipt):
        report = json.loads((products / "comparison.json").read_text())
        proof = json.loads(receipt.read_text())
        if not proof.get("float32_bodies_against_actual_raised_volumes"):
            raise ValueError("Complete saved raised Buildings reader required")
        rail = bound_products(report)
        data, state, baseline, buildings = inputs(products, receipt, allow_raised=True)
        for key in (
            "source_ids",
            "origin_lat_lon",
            "root_resident_sha256",
            "parent_reports",
            "support_sha256",
        ):
            if state[4][key] != rail.state[4][key]:
                raise ValueError(
                    "Raised/Buildings/Plants whole old state binding differs"
                )
        held["rail"] = rail
        return data, state, baseline, buildings

    def query_for(state, buildings):
        return candidate_query(
            state, buildings, extra_sources=supplements(held["rail"])
        )

    def reference_for(baseline, _state, buildings):
        return PlantQuery(
            ReferenceDistance(RailReference(baseline, held["rail"])), buildings
        )

    return prepare, query_for, reference_for, held


def rounded_envelopes(products, rail):
    report = json.loads((products / "comparison.json").read_text())
    building_root = Path(report["buildings"])
    occupied = list(whole_raised(rail, cast=True)[0])
    for i, body in enumerate(meshes(building_root / "buildings-after.npz")):
        _, footprint = body_query(f"rounded-building:{i}", quantized(body), 0)
        occupied.append(
            Reservation(
                footprint.source_id,
                footprint.geometry,
                footprint.heights,
                "building.body",
            )
        )
    query = OccupancyQuery(tuple(occupied))
    with np.load(products / "prototype/complete.npz", allow_pickle=False) as saved:
        original = saved["vertices"]
    accepted = json.loads((products / "plants.json").read_text())
    for i, plant in enumerate(accepted):
        vertices = (
            (original + plant["result"]["world_origin_m"]).astype("f4").astype(float)
        )
        footprint = bounds_query(
            f"rounded-whole-plant:{i}", [vertices.min(0), vertices.max(0)], [0, 0, 0]
        )
        cut, _work = query.conflict(footprint, method="scan")
        if not cut.is_empty:
            raise ValueError(
                "Actual rounded whole plant envelope enters raised transport/Buildings"
            )
    return len(accepted), len(original)


def run(args):
    prepare, query_for, reference_for, held = adapter()
    if args.products:
        report = json.loads((args.products / "comparison.json").read_text())
        if report.get("transport_model") != "raised-body-headroom-piers/1":
            raise ValueError("Explicit complete elevated plant input model required")
        with tempfile.TemporaryDirectory(
            prefix="outshine-plant-reader-", dir=args.output.parent
        ) as stage:
            result = verify_plants(
                args.products,
                Path(stage) / "partial.json",
                prepare=prepare,
                reference_for=reference_for,
            )
        count, vertices = rounded_envelopes(args.products, held["rail"])
        if result["report_sha256"] != digest(args.products / "comparison.json"):
            raise ValueError("Saved whole plant report changed during admission")
        result.update(
            actual_rounded_plants=count,
            rounded_vertices_per_plant=vertices,
            float32_envelopes_against_raised_and_buildings=True,
            scope="Whole original SITE/form/proposal/source/root/Double volume replay plus actual translated rounded prototype envelopes against rounded new rail/piers and all rounded Buildings. No full old-world Float32/native/Source/World/device admission.",
        )
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    else:
        from site_order import priority

        result = generate(
            args,
            order=partial(priority, mode="boundary"),
            order_id="site_order.boundary/1",
            prepare=prepare,
            query_for=query_for,
        )
        result["transport_model"] = "raised-body-headroom-piers/1"
        result["scope"] = (
            "Whole original SITE recipes after unchanged prior transport plus separately bound raised deck/pier/headroom and actual saved Buildings. No shared Plan/native/Source/World admission."
        )
        save(args.output, "comparison.json", result)
    return result


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path)
    parser.add_argument("--building-receipt", type=Path)
    parser.add_argument("--prototype", type=Path)
    parser.add_argument("--prototype-receipt", type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp output required")
    if not args.products and (args.buildings is None or args.building_receipt is None):
        parser.error("Generation requires --buildings and --building-receipt")
    result = run(args)
    print(
        json.dumps(
            {
                "whole_elevated_plant_products": "verified"
                if args.products
                else "generated"
            }
        )
    )
