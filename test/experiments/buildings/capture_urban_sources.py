"""Pin selected decoded vector semantics from explicit existing source receipts."""

import argparse
import gzip
import json
from pathlib import Path

from outshine_ref.places import decode_vector

LAYERS = {
    "landuse",
    "landcover",
    "park",
    "water",
    "waterway",
    "transportation",
    "aeroway",
}


def capture(receipt, *, parent=False):
    layers = decode_vector(receipt)
    selected = {"landuse"} if parent else LAYERS
    return {
        "tile": receipt["tile"],
        "source": {
            **{key: receipt[key] for key in ["url", "bytes", "sha256"]},
            "revision": receipt.get("revision", "pinned-uri"),
            "attribution": "OpenStreetMap contributors / OpenFreeMap / OpenMapTiles; test input, no photo textures",
        },
        "building_layer_present": "building" in layers,
        "original_layer_counts": {
            key: len(layer["features"]) for key, layer in layers.items()
        },
        "layers": {key: value for key, value in layers.items() if key in selected},
    }


def run(args):
    receipts = json.loads(args.receipts.read_text())
    result = capture(receipts["owner"])
    result["parents"] = [
        capture(parent, parent=True) for parent in receipts.get("urban_parents", [])
    ]
    payload = json.dumps(result, sort_keys=True, separators=(",", ":")).encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        raise ValueError("fresh explicit snapshot output required")
    args.output.write_bytes(gzip.compress(payload, mtime=0))
    print(
        json.dumps(
            {
                "snapshot_bytes": args.output.stat().st_size,
                "sources": 1 + len(result["parents"]),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
