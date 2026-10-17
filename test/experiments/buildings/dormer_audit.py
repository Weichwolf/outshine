"""Replay every actually admitted city roof and independently check physical products."""

import argparse
import hashlib
import json
import time
from dataclasses import asdict, replace
from pathlib import Path

import numpy as np
import shapely
from city_scene import inputs
from dormer_windows import DEPTH_M, aperture
from evidence import revision
from facade_parts import append
from provider_contract import attributes
from recipe_graph_data import plain
from showcase_tiles import view_for
from structural_shell_geometry import native
from tile_dormers import DormerFacades
from tile_roof_envelopes import choose


def audit(geometry):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the full-source physical audit first")
    report = json.loads(geometry.read_text())
    mesh = Path(report["mesh"])
    if hashlib.sha256(mesh.read_bytes()).hexdigest() != report["mesh_sha256"]:
        raise ValueError("Actual city mesh no longer matches the source receipt")
    admitted = {
        m["id"]: m
        for m in report["source"]["members"]
        if "roof_dormer_plan" in m["prototype"]
    }
    census = json.loads(
        Path(__file__)
        .with_name("data")
        .joinpath("roof-crown-city-window.json")
        .read_text()
    )
    rows, receipts, window = inputs("Paris", census)
    if receipts != report["sources"] or window != report["window"]:
        raise ValueError("Physical replay changed pinned provider inputs")
    owner = DormerFacades(rows, replace(view_for(rows), width=1920, height=1080))
    by_id = {r["id"]: r for r in owner.rows if r["layer"] == "building"}
    records = []
    started = time.perf_counter()
    for identifier, member in admitted.items():
        row = by_id[identifier]
        params = attributes(row["properties"])
        asset = owner.base.asset(row, params, choose(row, params))
        if asset is None:
            raise ValueError("Admitted source failed physical replay: " + identifier)
        plans = json.loads(
            json.dumps([asdict(p) for p in asset.dormers], default=plain)
        )
        if plans != member["prototype"]["roof_dormer_plan"]:
            raise ValueError("Replayed logical dormers changed: " + identifier)
        expected = asset.original.volume_m3 + sum(
            p.added_volume_m3 for p in asset.dormers
        )
        product = (
            asset.near_products()
            if member["prototype"]["roof_dormer_product"] == "near"
            else asset.full
        )
        if product is not asset.full:
            expected -= sum(
                aperture(p)[1] * aperture(p)[2] * DEPTH_M for p in asset.dormers
            )
        solid = native(product.physical.mesh, product.physical.vertex_roots)
        volume_error = abs(solid.volume() - expected)
        if volume_error > 1e-7:
            raise ValueError(
                "Independent roof/pocket integral disagrees: " + identifier
            )
        joined = native(append(product.body, [product.skin]))
        seam_xor = (joined - solid).volume() + (solid - joined).volume()
        if seam_xor > 1e-7:
            raise ValueError("Replayed body/roof seams changed volume: " + identifier)
        xyz = product.physical.mesh["vertices"][product.physical.mesh["faces"]]
        n = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        region = shapely.union_all(shapely.polygons(xyz[n[:, 2] > 0, :, :2]))
        original_region = shapely.from_wkb(asset.original.footprint_wkb)
        inherited_error = row["polygon"].symmetric_difference(original_region).area
        added_error = original_region.symmetric_difference(region).area
        footprint_error = row["polygon"].symmetric_difference(region).area
        if inherited_error > 1e-7 or footprint_error > 1e-7 or added_error > 1e-8:
            raise ValueError(
                "Physical roof filled a courtyard or left the source: " + identifier
            )
        height = float(product.physical.mesh["vertices"][:, 2].max())
        if height != params["buildingHeight"]:
            raise ValueError("Physical roof changed the original top: " + identifier)
        records.append(
            {
                "source_id": identifier,
                "dormers": len(asset.dormers),
                "mode": member["prototype"]["roof_dormer_product"],
                "physical_triangles": len(product.physical.mesh["faces"]),
                "volume_error_m3": volume_error,
                "body_skin_xor_m3": seam_xor,
                "footprint_error_m2": footprint_error,
                "inherited_roof_footprint_error_m2": inherited_error,
                "additional_union_footprint_error_m2": added_error,
                "top_m": height,
            }
        )
        if len(records) % 40 == 0:
            print(
                json.dumps({"checked": len(records), "total": len(admitted)}),
                flush=True,
            )
    return proof | {
        "source_geometry": str(geometry.resolve()),
        "source_geometry_sha256": hashlib.sha256(geometry.read_bytes()).hexdigest(),
        "records": records,
        "roofs": len(records),
        "dormers": sum(r["dormers"] for r in records),
        "seconds": time.perf_counter() - started,
        "scope": "Every actually admitted logical city roof independently replayed through the same current source/neighbor adapter; affine integral, pockets, closed products, matching seams, footprint/courtyards and exact top checked. Not per-building extraction from the already compiled whole-city draw or native/device/cache admission.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(audit(args.geometry), indent=2) + "\n")


if __name__ == "__main__":
    main()
