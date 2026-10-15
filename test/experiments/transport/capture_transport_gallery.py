"""Acquire four complete tile windows for the declared transport inspection gallery."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from capture_height_targets import sample_targets, world_coordinates
from capture_place_windows import acquire
from capture_transport_tile import capture

HERE = Path(__file__).resolve().parent
SPEC = HERE / "reference/transport-gallery.json"


def capture_case(row, spec, cache, dem_sources, output):
    origin, zoom = row["origin_lat_lon"], spec["zoom"]
    centre = np.floor(
        world_coordinates(origin, np.zeros((1, 2)))[0] * (1 << zoom)
    ).astype(int)
    tiles = row.get("tiles", [(zoom, *map(int, centre))])
    stage = output / "tile-capture"
    stage.mkdir(exist_ok=True)
    merged = {
        "origin_lat_lon": origin,
        "original_features": [],
        "deferred_sources": [],
        "roads": [],
        "receipts": [],
        "reserved_sources": [],
    }
    for tile in tiles:
        label = row["source"] + "-" + "-".join(map(str, tile))
        receipt = acquire(
            spec["provider_prefix"] + "/" + "/".join(map(str, tile)) + ".pbf", cache
        )
        receipt.update(tile=list(tile), sites=[label])
        inventory = stage / "inventory.json"
        inventory.write_text(
            json.dumps({"sites": [[label, *origin]], "receipts": [receipt]})
        )
        capture(label, inventory, cache, stage, zoom=zoom)
        source = json.loads((stage / (label + "-input.json")).read_text())
        for key in ("original_features", "deferred_sources", "roads"):
            merged[key].extend(source[key])
        merged["receipts"].append(source["receipt"])
    merged["original_axes"] = list(merged["roads"])
    datum, receipts = sample_targets(
        origin, np.zeros((1, 2)), dem_sources, 11, fetch=True
    )
    merged["gallery"] = dict(
        row, whole_tiles=tiles, datum_m=float(datum[0]), dem_receipts=receipts
    )
    path = output / "inputs" / (row["source"] + "-input.json")
    path.write_text(json.dumps(merged, separators=(",", ":")) + "\n")
    return {
        "source": row["source"],
        "datum_m": float(datum[0]),
        "whole_tiles": tiles,
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--dem-sources", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("fresh empty capture directory required")
    (args.output / "inputs").mkdir(parents=True)
    spec = json.loads(SPEC.read_text())
    rows = {
        r["id"]: capture_case(r, spec, args.cache, args.dem_sources, args.output)
        for r in spec["cases"]
    }
    (args.output / "capture.json").write_text(
        json.dumps(
            {
                "spec_sha256": hashlib.sha256(SPEC.read_bytes()).hexdigest(),
                "cases": rows,
            },
            indent=2,
        )
        + "\n"
    )
    (args.output / "inventory.json").write_text(
        json.dumps(
            {"sites": [[r["source"], *r["origin_lat_lon"]] for r in spec["cases"]]},
            indent=2,
        )
        + "\n"
    )
    shutil.rmtree(args.output / "tile-capture")
    print(json.dumps(rows), flush=True)


if __name__ == "__main__":
    main()
