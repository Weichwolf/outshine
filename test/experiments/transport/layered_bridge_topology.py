"""Source-bound one-way chains remain separate despite a shared physical road slab."""

import json
from pathlib import Path

import numpy as np
import shapely
from transport_travel import MODES, source_travel


def topology(fixture, axis):
    rows = fixture["sources"]
    recipes = json.loads(
        Path(__file__).with_name("transport_network_recipes.json").read_text()
    )
    public, restricted, notes = source_travel(rows, recipes["travelRecipes"])
    groups = {1: [], -1: []}
    for i, row in enumerate(rows):
        if row["profile"] != "motorway":
            if not np.all(public[i] & MODES["cycle"]):
                raise ValueError(
                    "lower selected source lacks two-way public cycling access"
                )
            continue
        if not public[i, 0] & MODES["motor"] or public[i, 1] & MODES["motor"]:
            raise ValueError(
                "selected motorway does not retain its supplied one-way travel"
            )
        xy = np.array(row["coordinates"])
        sign = int(np.sign((xy[-1] - xy[0]) @ axis))
        if not sign or row["width_m"] < 7:
            raise ValueError(
                "ambiguous selected direction or insufficient two-lane core"
            )
        groups[sign].append(row)
    chains, cores = {}, {}
    for sign, selected in groups.items():
        merged = shapely.line_merge(
            shapely.union_all([shapely.LineString(r["coordinates"]) for r in selected])
        )
        if merged.geom_type != "LineString":
            raise ValueError(
                "selected source endpoints do not form one complete directed carriageway"
            )
        cores[sign] = shapely.union_all(
            [
                shapely.LineString(r["coordinates"]).buffer(
                    3.5, cap_style="flat", join_style="mitre", mitre_limit=2
                )
                for r in selected
            ]
        )
        chains[sign] = {
            "xy": shapely.get_coordinates(merged),
            "source_ids": [r["source_id"] for r in selected],
        }
    overlap = float(cores[1].intersection(cores[-1]).area)
    if overlap > 1e-7:
        raise ValueError("opposite directed seven-metre traffic cores overlap")
    return chains, {
        "source_ids": [r["source_id"] for r in rows],
        "public_travel": public.tolist(),
        "restricted_travel": restricted.tolist(),
        "access_notes": notes,
        "opposite_core_overlap_m2": overlap,
        "core_width_m": 7.0,
        "logical_cross_direction_connections": 0,
        "scope": "Two supplied one-way source chains and one lower two-way cycle/walk source group; physical shoulder overlap creates no turn. No complete lane/turn/swept-vehicle route certificate.",
    }
