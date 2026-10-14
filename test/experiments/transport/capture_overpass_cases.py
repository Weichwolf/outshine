"""Explicitly derive portable level cases from already pinned complete source tiles."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import sample_targets
from surface_height_frames import RECIPES
from transport_building_clearance import street_kind

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = Path(__file__).resolve().parent / "fixtures/overpass-vienna-cases.json"


def capture(fetch):
    sources = (
        Path.home()
        / ".local/share/outshine/outshine/sources/transport-experiments/mapterhorn"
    )
    rows, receipts = [], []
    for name in ("Vienna", "Vienna-Donau"):
        path = ROOT / f"build/experiments/transport/inputs/{name}-input.json"
        raw = json.loads(path.read_text())
        roads = raw["roads"]
        lines = np.array([shapely.LineString(r["coordinates"]) for r in roads], object)
        index = shapely.STRtree(lines)
        selected = {}
        for i, upper in enumerate(roads):
            if upper["properties"].get("brunnel") != "bridge":
                continue
            for j in sorted(index.query(lines[i], predicate="crosses")):
                lower = roads[j]
                if lower["properties"].get("brunnel") in ("bridge", "tunnel") or lower[
                    "properties"
                ].get("indoor"):
                    continue
                if lines[i].intersection(lines[j]).geom_type != "Point":
                    continue
                if len(rows) >= 256:
                    raise ValueError("capture exceeds level-case scope")
                pair = []
                for source in (upper, lower):
                    source_id = source["source_id"]
                    if source_id not in selected:
                        kind = street_kind(source["properties"])
                        rail = source["properties"].get("subclass") in (
                            "rail",
                            "subway",
                            "tram",
                        ) or source["properties"].get("class") in ("rail", "transit")
                        xy = np.asarray(source["coordinates"])
                        heights, dem = sample_targets(
                            raw["origin_lat_lon"], xy[[0, -1]], sources, 11, fetch
                        )
                        selected[source_id] = {
                            "source_id": source_id,
                            "properties": source["properties"],
                            "xy": xy.tolist(),
                            "end_heights_m": heights.tolist(),
                            "width_m": source["width_m"] or 4.0,
                            "width_assumed": True,
                            "width_origin": "class_recipe"
                            if source["width_m"]
                            else "fallback_4m",
                            "crossfall": 0.0 if rail else 0.015,
                            "grade": 0.03 if rail else RECIPES.get(kind, 0.08),
                            "dem": [
                                {
                                    k: v
                                    for k, v in row.items()
                                    if k != "acquisition_and_decode_ms"
                                }
                                for row in dem
                            ],
                        }
                    pair.append(selected[source_id])
                rows.append(
                    {
                        "id": f"{name}-crossing-{len(rows)}",
                        "upper": pair[0],
                        "lower": pair[1],
                    }
                )
        receipts.append(
            {
                "site": name,
                "source_input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "receipt": raw["receipt"],
                "origin_lat_lon": raw["origin_lat_lon"],
                "axis_parts": len(roads),
                "original_features": len(raw["original_features"]),
                "deferred_sources": len(raw["deferred_sources"]),
                "considered_axis_ids": sorted(selected),
                "unselected_axis_ids": [
                    r["source_id"] for r in roads if r["source_id"] not in selected
                ],
                "unselected_reason": "outside_selected_local_pair_scope",
                "deferred_source_ids": [
                    r["source_id"] for r in raw["deferred_sources"]
                ],
                "deferred_reason": "polygon_source_has_no_axis_recipe",
            }
        )
    OUTPUT.write_text(
        json.dumps(
            {
                "cases": rows,
                "inputs": receipts,
                "selection": "All bridge versus non-bridge/non-tunnel/non-indoor proper single centreline crossings in two pinned Vienna tiles. Boundary joins and multiple-crossing pairs need different recipes; this is a selected local component experiment.",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                "cases": len(rows),
                "fixture": str(OUTPUT),
                "sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fetch",
        action="store_true",
        help="Acquire missing DEM tiles only; retained tiles are never replaced",
    )
    capture(parser.parse_args().fetch)
