"""Explicit pinned-tile capture: whole connected deck and all area conflicts."""

import argparse
import collections
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import sample_targets
from shared_bridge_geometry import corridor

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = Path(__file__).with_name("fixtures") / "shared-bridge-vienna.json"


def capture():
    pair_fixture = Path(__file__).with_name("fixtures") / "overpass-vienna-cases.json"
    pairs = json.loads(pair_fixture.read_text())
    counts = collections.Counter(c["upper"]["source_id"] for c in pairs["cases"])
    seed = max(sorted(counts), key=counts.get)
    receipt = next(i for i in pairs["inputs"] if seed in i["considered_axis_ids"])
    path = ROOT / f"build/experiments/transport/inputs/{receipt['site']}-input.json"
    raw = json.loads(path.read_text())
    roads = raw["roads"]
    shapes = [
        corridor({"xy": r["coordinates"], "width_m": r["width_m"] or 4}) for r in roads
    ]
    bridges = [
        i for i, r in enumerate(roads) if r["properties"].get("brunnel") == "bridge"
    ]
    chosen = {i for i, r in enumerate(roads) if r["source_id"] == seed}
    for _ in range(64):
        domain = shapely.union_all([shapes[i] for i in sorted(chosen)])
        expanded = chosen | {
            i
            for i in bridges
            if shapes[i].intersection(domain).area > 0
            or shapes[i].intersection(domain).length > 1e-7
        }
        if expanded == chosen:
            break
        chosen = expanded
        if len(chosen) > 64:
            raise ValueError("deck source capture budget")
    else:
        raise ValueError("deck expansion budget")
    conflicts = [
        i
        for i in range(len(roads))
        if i not in chosen and shapes[i].intersection(domain).area > 0
    ]
    if len(conflicts) > 64:
        raise ValueError("ground conflict capture budget")
    cache = (
        Path.home()
        / ".local/share/outshine/outshine/sources/transport-experiments/mapterhorn"
    )
    selected = []
    for i in sorted(chosen | set(conflicts)):
        source = roads[i]
        xy = np.asarray(source["coordinates"])
        heights, dem = sample_targets(
            raw["origin_lat_lon"], xy[[0, -1]], cache, 11, False
        )
        selected.append(
            {
                "source_id": source["source_id"],
                "xy": xy.tolist(),
                "width_m": source["width_m"] or 4.0,
                "width_origin": "class_recipe" if source["width_m"] else "fallback_4m",
                "width_assumed": True,
                "properties": source["properties"],
                "end_heights_m": heights.tolist(),
                "dem": [
                    {k: v for k, v in row.items() if k != "acquisition_and_decode_ms"}
                    for row in dem
                ],
                "role": "deck"
                if i in chosen
                else "deferred"
                if source["properties"].get("indoor")
                else "ground",
                "omission_reason": "indoor_surface_requires_buildings_contact"
                if source["properties"].get("indoor")
                else "",
            }
        )
    document = {
        "sources": selected,
        "seed_source_id": seed,
        "source_input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "pair_fixture_sha256": hashlib.sha256(pair_fixture.read_bytes()).hexdigest(),
        "receipt": raw["receipt"],
        "origin_lat_lon": raw["origin_lat_lon"],
        "all_axis_source_ids": [r["source_id"] for r in roads],
        "unselected_reason": "outside_connected_bridge_component_area_scope",
        "deferred_source_ids": [r["source_id"] for r in raw["deferred_sources"]],
        "deferred_reason": "polygon_source_has_no_axis_recipe",
        "scope": "Seed is the pinned pair fixture's most-crossed bridge. Expand all positively overlapping or shared-edge bridge corridors; include every other positively overlapping corridor from the complete source tile, including footprint-only conflicts. Further ground neighbors, tile neighbors and traffic routes are not certified.",
    }
    FIXTURE.write_text(json.dumps(document, indent=2) + "\n")
    print(
        json.dumps(
            {
                "fixture": str(FIXTURE),
                "sources": len(selected),
                "deck_sources": len(chosen),
                "ground_sources": sum(s["role"] == "ground" for s in selected),
                "deferred_indoor_sources": sum(
                    s["role"] == "deferred" for s in selected
                ),
                "sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
            }
        )
    )


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    capture()
