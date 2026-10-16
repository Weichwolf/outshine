"""Master's unchanged three-tile fixture, owner clipping before any root sampling."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from habitat_stand_experiment import support_bodies
from outshine_ref import LocalFrame
from outshine_ref.vegetation import HabitatStand, StandHabitat
from outshine_ref.vegetation.stand_habitat import polygon_components
from shapely.affinity import affine_transform

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "test/outshine/data/vegetation/feldkirch-habitats.json"


def prepared():
    source = json.loads(SOURCE.read_text())
    camera = source["camera"]
    latitude = np.deg2rad(camera["latitude_deg"])
    scale = np.cos(latitude)
    radius = 6378137.0
    origin_x = radius * np.deg2rad(camera["longitude_deg"])
    origin_y = radius * np.log(np.tan(np.pi / 4 + latitude / 2))
    frame = LocalFrame("feldkirch-local-mercator-cos-camera-latitude")
    for tile in source["tiles"]:
        z, x, y = tile["tile"]
        extent = tile["extent"]
        width = 2 * np.pi * radius / 2**z
        transform = (
            width * scale / extent,
            0,
            0,
            -width * scale / extent,
            ((x / 2**z - 0.5) * 2 * np.pi * radius - origin_x) * scale,
            ((0.5 - y / 2**z) * 2 * np.pi * radius - origin_y) * scale,
        )
        for feature in tile["features"]:
            original = shapely.geometry.shape(feature["geometry"])
            owner = original.intersection(shapely.box(0, 0, extent, extent))
            # GEOS may retain isolated zero-area contacts at tile boundaries.
            owner = shapely.MultiPolygon(polygon_components(owner))
            habitat = StandHabitat.prepare(
                frame,
                f"openfreemap:{feature['id']}",
                f"owner:{z}/{x}/{y}",
                affine_transform(original, transform),
                (-4096, -4096),
                8192,
                support=affine_transform(owner, transform),
            )
            # Explicit synthetic stock solely for this algorithm control.
            count = int(habitat.area_m2 * 0.04)
            yield (
                feature,
                HabitatStand(
                    habitat, ("generic-tree",), (count,), (20.0,), (5.0,), levels=6
                ),
            )


def run(output, preview=None):
    records, domains, namespaces = [], [], set()
    preview_fields = []
    holes = 0
    for feature, plan in prepared():
        holes += sum(
            len(p.interiors)
            for p in shapely.get_parts(shapely.geometry.shape(feature["geometry"]))
        )
        root = plan.field()
        fields = [plan.field((3, x, y)) for x in range(8) for y in range(8)]
        np.testing.assert_array_equal(
            np.sum([f["counts"] for f in fields], axis=0), root["counts"]
        )
        assert not plan._sites
        assert len(plan._cells) == 85
        if preview is not None:
            preview_fields.extend(fields)
        assert plan.layout_identity not in namespaces
        namespaces.add(plan.layout_identity)
        key = (0, 0, 0)
        for _ in range(plan.levels):
            key = max(plan.children(key), key=lambda c: (sum(c.counts), c.key)).key
        sites = plan.sites(key)
        assert sites is not None
        assert shapely.covers(
            root["domain"], shapely.points(sites["positions_xy_m"])
        ).all()
        records.append(
            {
                "provider_id": feature["id"],
                "source_id": plan.habitat.source_id,
                "region_id": plan.habitat.region_id,
                "source_area_m2": plan.habitat.source_area_m2,
                "owned_area_m2": root["area_m2"],
                "counts": plan.counts,
                "coarse_cells": 85,
                "coarse_sites": 0,
                "one_near_key": key,
                "one_near_sites": len(sites["ids"]),
            }
        )
        domains.append(root["domain"])
    area = sum(d.area for d in domains)
    union = shapely.union_all(domains)
    assert abs(area - union.area) < 1e-6
    result = {
        "input": str(SOURCE.relative_to(ROOT)),
        "frame": "Shared affine Mercator plane, camera-latitude ground scale; approximate metres, no DEM/ENU-height claim.",
        "records": records,
        "raw_area_m2": sum(r["source_area_m2"] for r in records),
        "owned_area_m2": area,
        "owned_union_area_m2": union.area,
        "original_holes": holes,
        "scope": "37 source records, owner tile buffers removed, provider IDs retained. Generic stock is authored, not observed species/density; no native coverage/render admission.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    if preview is not None:
        preview.mkdir(parents=True, exist_ok=False)
        np.savez_compressed(
            preview / "coarse-support.npz", **support_bodies(preview_fields)
        )
        (preview / "summary.json").write_text(
            json.dumps(
                {
                    "source_id": "37 owned Feldkirch source records",
                    "scope": "Source-support diagnostics only; flat zero datum, constant descriptor heights, no tree/organ/canopy rendering.",
                },
                indent=2,
            )
            + "\n"
        )
    print(json.dumps({k: v for k, v in result.items() if k != "records"}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    run(args.output, args.preview)
