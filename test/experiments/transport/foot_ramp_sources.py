"""Existing source/access semantics bound to actual used ground graph edges."""

import json
from pathlib import Path

import numpy as np
from curb_plan import URBAN
from rail_graph_inputs import read_window
from tilted_rail_inputs import PINS
from transport_travel import MODES, source_travel

HERE = Path(__file__).resolve().parent


def source_edges(network):
    _source, raw, graphs, _bed, _binding = read_window(
        network, "ViennaNetwork", files=PINS
    )
    usage = json.loads(
        (network / "results/ViennaNetwork/components/source-usage.json").read_text()
    )
    used = {r["id"] for r in usage if r["status"] != "not_used"}
    ids = sorted(raw)
    recipes = json.loads((HERE / "transport_network_recipes.json").read_text())[
        "travelRecipes"
    ]
    public, _restricted, notes = source_travel([raw[s] for s in ids], recipes)
    walking = {
        s for s, mode in zip(ids, public, strict=True) if np.any(mode & MODES["walk"])
    }
    feet, roads, ledger = [], [], []
    for graph in graphs:
        for index, line in enumerate(graph["lines"]):
            owners = [s for s in graph["sources"][index] if s in used]
            if not owners:
                continue
            properties = [raw[s]["properties"] for s in owners]
            ground = all(
                not p.get("indoor")
                and not p.get("layer", 0)
                and not p.get("level", 0)
                and p.get("brunnel") not in ("bridge", "tunnel")
                for p in properties
            )
            foot = all(
                p.get("class") == "path"
                and p.get("subclass") in ("footway", "pedestrian", "path")
                for p in properties
            )
            road = all(
                p.get("class") in URBAN and p.get("surface") == "paved"
                for p in properties
            )
            if not (foot or road):
                continue
            reason = "" if ground else "not_explicit_ground_source"
            if foot and not all(s in walking for s in owners):
                reason = "no_recipe_public_walk_access"
            coordinates = np.asarray(line.coords)
            if len(coordinates) != 2 or line.length < 8:
                reason = reason or "not_long_straight_local_edge"
            if reason:
                if foot:
                    ledger.append({"sources": owners, "reason": reason})
                continue
            row = {
                "line": line,
                "xy": coordinates,
                "sources": owners,
                "half": float(graph["half_widths"][index]),
                "access_basis": [
                    "supplied_tags"
                    if any(k in p for k in ("access", "foot"))
                    else "procedural_recipe"
                    for p in properties
                ],
            }
            (feet if foot else roads).append(row)
    if len(feet) + len(roads) > 65536:
        raise ValueError("bounded ramp source graph required")
    notes = [dict(n, source_id=ids[n["source"]]) for n in notes]
    return (
        feet,
        roads,
        ledger,
        {
            "eligible_foot_edges": len(feet),
            "urban_edges": len(roads),
            "source_access_notes": notes,
        },
    )
