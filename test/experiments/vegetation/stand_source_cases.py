"""Extract pinned real forest boundaries from existing local MVT bytes, without IO acquisition."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from outshine_ref import LocalFrame
from outshine_ref.places import decode_vector, read_camera, tile_geography
from outshine_ref.vegetation import StandHabitat

ROOT = Path(__file__).resolve().parents[3]


def extract(cache_root, sources, output):
    inventory = json.loads(sources.read_text())
    record = next(row for row in inventory["cases"] if row["place"] == "Feldkirch")
    receipt = record["receipt"]
    name = hashlib.sha256(receipt["url"].encode()).hexdigest() + ".pbf"
    candidates = cache_root.rglob(name)
    path = next(
        (
            p
            for p in candidates
            if hashlib.sha256(p.read_bytes()).hexdigest() == receipt["sha256"]
        ),
        None,
    )
    if path is None:
        raise ValueError(
            "exact pinned forest source not present; no download or pin update"
        )
    tile = decode_vector(dict(receipt, path=str(path), kind="vector"))
    camera = read_camera(ROOT / "src/assets/places/Feldkirch.scenario")
    frame = LocalFrame(
        f"enu:{camera.origin_latitude_deg:.12g}:{camera.origin_longitude_deg:.12g}:ellipsoid0"
    )
    layer = tile["landcover"]
    results = []
    for identity in (2824093, 67435063, 2999194372):
        matches = [
            (i, f) for i, f in enumerate(layer["features"]) if f.get("id") == identity
        ]
        if len(matches) != 1:
            raise ValueError("fixed unique forest feature missing")
        index, feature = matches[0]
        if feature["properties"].get("class") != "wood":
            raise ValueError("fixed source class changed")
        raw = shapely.geometry.shape(feature["geometry"])

        def metric(xy):
            latitude, longitude = tile_geography(xy, layer["extent"], receipt["tile"])
            return camera.local(latitude, longitude, np.zeros(len(xy)))[:, :2]

        geometry = shapely.transform(raw, metric)
        source_id = (
            "openfreemap:"
            + "/".join(str(v) for v in receipt["tile"])
            + ":landcover:"
            + str(identity)
        )
        habitat = StandHabitat.prepare(
            frame, source_id, "Feldkirch-enu-root-4096", geometry, (-2048, -2048), 4096
        )
        count = round(habitat.area_m2 * 0.04)
        broadleaf = round(count * 0.7)
        results.append(
            {
                "type": "Feature",
                "id": source_id,
                "geometry": shapely.geometry.mapping(geometry),
                "properties": {
                    "frame": frame.identifier,
                    "source_id": source_id,
                    "region_id": habitat.region_id,
                    "provider": "OpenFreeMap",
                    "url": receipt["url"],
                    "source_sha256": receipt["sha256"],
                    "tile": receipt["tile"],
                    "layer": "landcover",
                    "feature_id": identity,
                    "feature_ordinal": index,
                    "source_properties": feature["properties"],
                    "projection": "Shared PlaceCamera ECEF->ENU XY at ellipsoid0; flat research datum, no DEM.",
                    "region_lower_xy_m": [-2048, -2048],
                    "region_side_m": 4096,
                    "expected_source_area_m2": habitat.source_area_m2,
                    "expected_root_area_m2": habitat.area_m2,
                    "kinds": ["generic-broadleaf", "generic-conifer"],
                    "counts": [broadleaf, count - broadleaf],
                    "heights_m": [18.0, 22.0],
                    "spreads_m": [6.0, 5.0],
                    "levels": 6,
                    "seed": 718,
                    "procedural": "Counts and kind mixture are authored from0.04 roots per planar m2; not observed stock or species. Exact original source geometry/properties/IDs retained.",
                },
            }
        )
    result = {
        "type": "FeatureCollection",
        "format": "habitat-stand-cases-1",
        "source_inventory_sha256": hashlib.sha256(sources.read_bytes()).hexdigest(),
        "features": results,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            [
                {
                    "id": f["id"],
                    "area_m2": f["properties"]["expected_root_area_m2"],
                    "counts": f["properties"]["counts"],
                }
                for f in results
            ]
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-root", type=Path, required=True)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    extract(args.cache_root, args.sources, args.output)
