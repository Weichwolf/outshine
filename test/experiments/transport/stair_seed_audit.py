"""Independent actual seed source graph, whole profiles, body and separation checks."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_cycle_contacts import profile
from stair_junction_layout import landings
from stair_module_audit import inspect_pins, module_body, traffic
from stair_module_clearance import source_clearances
from stair_module_materials import surface_slots
from stair_module_node_heights import apply_heights, inspect_heights
from stair_module_profiles import new_interior_checks, owned
from stair_module_roads import inspect_frames
from stair_module_trace import compare
from stair_seed_geometry import plans_for


def connected(recipe, rows, nodes):
    keys = [r["source_id"] for r in rows]
    seen = {keys.index(recipe["seed"]["source_id"])}
    for _iteration in range(8):
        before = set(seen)
        for node in nodes:
            if seen.intersection(node["incident"]):
                seen.update(node["incident"])
        if seen == before:
            break
    if seen != set(range(len(rows))):
        raise ValueError("independent seed has disconnected original source membership")


def profiles(data, rows, nodes, masks, original):
    internal, outer = [], []
    for owner, row in enumerate(rows):
        own = owned(data, owner, [], None)
        for end in (0, -1):
            incident = [
                n
                for n in nodes
                if owner in n["incident"] and np.array_equal(n["xy"], row["xy"][end])
            ]
            if len(incident) > 1:
                raise ValueError("ambiguous original seed endpoint")
            others = [
                cap
                for node in incident
                for neighbour in node["incident"]
                if neighbour != owner
                for cap in owned(data, neighbour, [], None)
            ]
            proof = {
                "source_id": row["source_id"],
                "end": end,
                "reference": "other_original_sources"
                if incident
                else "single_owned_outer_coverage",
                "checks": [
                    compare([own, others if incident else own], profile(row, end), cast)
                    for cast in (False, True)
                ],
            }
            (internal if incident else outer).append(proof)
    return {
        "new_internal_profiles": internal,
        "outer_profile_coverage": outer,
        "new_interior_profiles": new_interior_checks(
            data, rows, nodes, masks, original, {}
        ),
    }


def overlap_contract(nodes, domains, masks):
    for i, a in enumerate(domains):
        for j, b in enumerate(domains[:i]):
            common = a.intersection(b)
            allowed = shapely.union_all(
                [
                    mask
                    for node, mask in zip(nodes, masks, strict=True)
                    if i in node["incident"] and j in node["incident"]
                ]
            )
            if common.difference(allowed).area > 1e-7:
                raise ValueError(
                    "independent seed requires a declared original junction for band overlap"
                )


def audit(state, recipe, data, rows, planning, upper):
    if any(row.get("source_grid_m") != recipe.get("seed_grid_m") for row in rows):
        raise ValueError("seed source grid differs from its declared recipe")
    nodes, original, domains, masks, _old = planning
    expected_masks = landings(rows, original, "width", 0.75, nodes)[2]
    if len(masks) != len(expected_masks) or any(
        not a.equals(b) for a, b in zip(masks, expected_masks, strict=True)
    ):
        raise ValueError("saved seed junction masks differ from original node geometry")
    if not np.array_equal(
        data["surface_materials"], surface_slots(rows, data["owners"])
    ):
        raise ValueError("saved seed source material ownership changed")
    connected(recipe, rows, nodes)
    overlap_contract(nodes, domains, masks)
    separation = []
    road = state["road"]
    for cast in (False, True):
        vertices = (
            data["surface_vertices"].astype("f4").astype(float)
            if cast
            else data["surface_vertices"]
        )
        old_vertices = (
            road["body_vertices"].astype("f4").astype(float)
            if cast
            else road["body_vertices"]
        )
        new = shapely.union_all(shapely.polygons(vertices[data["surface_faces"], :2]))
        previous = shapely.union_all(
            shapely.polygons(old_vertices[road["top_faces"][state["road_ground"]], :2])
        )
        distance = float(new.distance(previous)) if not previous.is_empty else None
        if distance is not None and distance < 0.2:
            raise ValueError("actual independent seed touches old ground traffic")
        separation.append({"float32": cast, "minimum_old_ground_distance_m": distance})
    if any(not p.equals(q) for p, q in zip(original, domains, strict=True)):
        raise ValueError("independent seed borrowed an undeclared old surface")
    plans = plans_for(rows, nodes)
    fixed, values = apply_heights(
        data["surface_vertices"][:, :2],
        np.zeros(len(data["surface_vertices"]), bool),
        data["targets"],
        plans,
    )
    inspect_pins(data, fixed, values, "seed")
    clearances = source_clearances(data, rows, original, [], upper)
    grid = recipe.get("seed_grid_m")
    raw = (
        [
            corridor({k: v for k, v in row.items() if k != "source_grid_m"})
            for row in rows
        ]
        if grid
        else domains
    )
    return {
        "body": module_body(
            data, rows, raw, source_tolerance_m=6 * grid if grid else 0.0
        ),
        "profiles": profiles(data, rows, nodes, masks, original),
        "new_junction_heights": inspect_heights(data, plans),
        "road_frames": [inspect_frames(data, rows, cast) for cast in (False, True)],
        "old_ground_separation": separation,
        "source_clearances": clearances,
        "traffic": traffic(data, upper, state["piers"], clearances=clearances),
        "source_footprint_model": [
            {
                "source_id": row["source_id"],
                "grid_m": row.get("source_grid_m"),
                "original_to_model_hausdorff_m": shape.hausdorff_distance(
                    corridor({k: v for k, v in row.items() if k != "source_grid_m"})
                ),
                "whole_original_profiles_required": True,
            }
            for row, shape in zip(rows, original, strict=True)
        ],
    }
