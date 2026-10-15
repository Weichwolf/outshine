"""Capture one already pinned OpenFreeMap tile offline, preserving every source feature."""

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from outshine_ref.places import decode_vector
from transport_network import transport_widths

ROOT = Path(__file__).resolve().parents[3]
GIRTH_M = 40075016.68557849


def capture(site, inventory_path, cache, output):
    inventory = json.loads(inventory_path.read_text())
    receipts = sorted(
        (r for r in inventory["receipts"] if site in r["sites"] and r["tile"][0] == 14),
        key=lambda r: r["tile"],
    )
    if not receipts:
        raise ValueError("No pinned source tile for " + site)
    receipt = receipts[len(receipts) // 2]
    path = cache / (hashlib.sha256(receipt["url"].encode()).hexdigest() + ".pbf")
    if not path.exists():
        path = Path(receipt.get("path", path))
    _, latitude, longitude = next(s for s in inventory["sites"] if s[0] == site)
    z, tx, ty = receipt["tile"]
    span = GIRTH_M / (1 << z)
    scale = math.cos(math.radians(latitude))
    anchor = np.array(
        [
            longitude / 360 * GIRTH_M,
            math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi),
        ]
    )
    layer = decode_vector(dict(receipt, kind="vector", path=str(path)))[
        "transportation"
    ]
    extent = layer["extent"]
    widths = transport_widths()
    roads, deferred = [], []
    for feature in layer["features"]:
        geometry = feature["geometry"]
        source = f"openfreemap:{z}/{tx}/{ty}:transportation:{feature['id']}"
        if geometry["type"] not in ("LineString", "MultiLineString"):
            deferred.append(
                {
                    "source_id": source,
                    "reason": "polygon_source_has_no_axis_recipe",
                    "original": feature,
                }
            )
            continue
        parts = (
            geometry["coordinates"]
            if geometry["type"] == "MultiLineString"
            else [geometry["coordinates"]]
        )
        for ordinal, points in enumerate(parts):
            coordinates = (
                (np.asarray(points) / extent + [tx, ty]) * [span, -span]
                + [-GIRTH_M / 2, GIRTH_M / 2]
                - anchor
            ) * scale
            roads.append(
                {
                    "source_id": f"{source}:{ordinal}",
                    "coordinates": coordinates.tolist(),
                    "properties": feature["properties"],
                    "width_m": widths.get(feature["properties"].get("class")),
                }
            )
    captured = {
        "receipt": dict(receipt, path=str(path)),
        "origin_lat_lon": [latitude, longitude],
        "scope": "One complete pinned tile; flat datum is a recipe assumption, not DEM.",
        "original_features": layer["features"],
        "deferred_sources": deferred,
        "roads": roads,
    }
    output.mkdir(parents=True, exist_ok=True)
    destination = output / f"{site}-input.json"
    destination.write_text(json.dumps(captured, separators=(",", ":")) + "\n")
    print(
        json.dumps(
            {
                "site": site,
                "source_features": len(layer["features"]),
                "axis_parts": len(roads),
                "deferred_polygons": len(deferred),
                "input": str(destination),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", default="Vienna")
    parser.add_argument(
        "--inventory",
        type=Path,
        default=ROOT / "doc/references/data/osm/openfreemap-inventory-20261009.json",
    )
    parser.add_argument(
        "--cache",
        type=Path,
        default=Path.home()
        / ".local/share/outshine/outshine/sources/building-experiments/openfreemap",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    capture(args.site, args.inventory, args.cache, args.output)


if __name__ == "__main__":
    main()
