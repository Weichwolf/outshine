"""Explicitly preserve actual native source DTOs and bound DEM as reference inputs.

This copies inputs, never expected port outputs. Normal tests only verify/read
the captured bytes. The native private DTO dump is not a product/cache codec.
"""

import argparse
import gzip
import hashlib
import json
from pathlib import Path

from outshine_ref.places import decode_vector


def digest(data):
    return hashlib.sha256(data).hexdigest()


def recipe_rows(snapshot, payload, profiles_path):
    layer = decode_vector(
        {
            "kind": "vector",
            "path": str(payload),
            "sha256": snapshot["input_sha256"],
        }
    )["transportation"]
    features = {}
    for feature in layer["features"]:
        features.setdefault(feature.get("id"), []).append(feature)
    profiles = {
        row["kind"]: row
        for template in json.loads(profiles_path.read_text())["templates"]
        for row in template.get("osm", [])
        if row.get("layer") == "streets"
    }
    result = {}
    for path in snapshot["Paths"]:
        reference = snapshot["References"][path["FirstReference"]]
        found = features.get(reference["ProviderElementId"], [])
        properties = found[0]["properties"] if len(found) == 1 else {}
        candidates = (
            properties.get("subclass"),
            properties.get("class"),
            "unclassified",
        )
        kind = next(value for value in candidates if value in profiles)
        gradient = (
            profiles.get(kind, {}).get("maxGradient") if len(found) == 1 else None
        )
        result[str(path["PathId"])] = {
            "source_kind": kind,
            "source_properties": properties,
            "maximum_gradient": gradient,
            "origin": "existing class recipe"
            if gradient
            else "missing unambiguous recipe",
        }
    return result


def capture(args):
    if args.output.exists():
        raise ValueError("fresh explicit reference input directory required")
    manifest = json.loads(args.manifest.read_text())
    raw = args.snapshot.read_bytes()
    if len(raw) != manifest["bytes"] or digest(raw) != manifest["json_sha256"]:
        raise ValueError("actual native source export changed")
    snapshot = json.loads(raw)
    if snapshot["schema"] != "outshine/native-mvt-snapshot-dump/1":
        raise ValueError("known private native DTO capture required")
    if digest(args.payload.read_bytes()) != manifest["input_sha256"]:
        raise ValueError("original source payload changed")
    if snapshot["input_sha256"] != manifest["input_sha256"]:
        raise ValueError("native export and original payload binding differ")
    dem = args.dem.read_bytes()
    if digest(dem) != args.dem_sha256:
        raise ValueError("explicit original DEM pin required")
    recipes = recipe_rows(snapshot, args.payload, args.profiles)
    args.output.mkdir(parents=True)
    files = {
        "native.json.gz": gzip.compress(raw, compresslevel=9, mtime=0),
        "dem.webp": dem,
        "recipes.json": (
            json.dumps(recipes, sort_keys=True, separators=(",", ":")) + "\n"
        ).encode(),
    }
    for name, data in files.items():
        (args.output / name).write_bytes(data)
    metadata = {
        "schema": "outshine/canonical-port-reference-inputs/1",
        "native_capture": manifest,
        "native_schema": snapshot["schema"],
        "native_raw_sha256": digest(raw),
        "native_raw_bytes": len(raw),
        "files": {
            name: {"bytes": len(data), "sha256": digest(data)}
            for name, data in files.items()
        },
        "recipe_source_sha256": digest(args.profiles.read_bytes()),
        "terrain": {
            "tile": snapshot["tile"],
            "source_sha256": digest(dem),
            "source_url": "https://tiles.mapterhorn.com/"
            + "/".join(map(str, snapshot["tile"]))
            + ".webp",
            "source_datum": "Mapterhorn values as supplied; no measured geoid/ellipsoid equivalence",
            "height_mapping": "source signed metres as model geodetic heights/1",
            "sampling": "bilinear boundary posts, side-1",
        },
        "credits": "Vectors: OpenMapTiles/OpenStreetMap contributors; elevation: Mapterhorn. Original source tables and capture receipts retained.",
        "scope": "Real source-only native DTO input, raw bound DEM and existing recipes. No cooked heights, expected new ports, invented OSM mapping, product codec or full-world admission.",
    }
    (args.output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(args.output),
                "bytes": sum(len(v) for v in files.values()),
                "paths": len(recipes),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("snapshot", "manifest", "payload", "dem", "profiles", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--dem-sha256", required=True)
    capture(parser.parse_args())
