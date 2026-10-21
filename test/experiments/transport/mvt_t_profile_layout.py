"""Adapt retained source parts and the explicit 2D proposals to shared surfaces."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_junction_layout import layout
from stair_seed_precision import GRID_M, outward_contract


def prior_values(row, xy):
    original = row["height_prior_xyz_m"]
    line = shapely.LineString(original[:, :2])
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(original[:, :2], axis=0), axis=1))]
    return np.interp(
        shapely.line_locate_point(line, shapely.points(xy)), arc, original[:, 2]
    )


def freeze_regions(plan):
    """Freeze XY output domains and landing patches before any height solution."""
    domains = [
        shapely.union_all(shapely.polygons(plan["xy"][f])) for f in plan["parts"]
    ]
    footprint_checks = []
    for row, domain in zip(plan["rows"], domains, strict=True):
        raw = corridor({k: v for k, v in row.items() if k != "source_grid_m"})
        if plan["mesh_recipe"] == "outward_binary_grid":
            footprint_checks.append(outward_contract(domain, raw, 6 * GRID_M))
        elif domain.symmetric_difference(raw).area > 1e-7:
            raise ValueError("source mesh changed its unrounded source corridor")
    patches, errors = [], []
    for mask, owners in zip(plan["masks"], plan["incident"], strict=True):
        cells = np.concatenate(
            [shapely.polygons(plan["xy"][plan["parts"][i]]) for i in owners]
        )
        selected = shapely.covers(mask, shapely.point_on_surface(cells))
        patch = shapely.union_all(cells[selected])
        intended = mask.intersection(shapely.union_all([domains[i] for i in owners]))
        bound = 6 * GRID_M if plan["mesh_recipe"] == "outward_binary_grid" else 1e-7
        if (
            patch.is_empty
            or not patch.buffer(bound).covers(intended)
            or not intended.buffer(bound).covers(patch)
        ):
            raise ValueError(
                "prepared landing leaves its declared XY displacement bound"
            )
        patches.append(patch)
        errors.append(patch.hausdorff_distance(intended))
    plan["prepared_domains"] = domains
    plan["input_landing_masks"] = plan["masks"]
    plan["masks"] = patches
    plan["footprint_checks"] = footprint_checks
    plan["landing_displacement_m"] = errors


def prepare_profiles(
    case, sources, proposal, *, landing_reach_m=0.75, mesh_recipe="source"
):
    if not 1 <= len(sources) <= 64 or not 0 <= landing_reach_m <= 2:
        raise ValueError("bounded sources and declared landing reach required")
    if mesh_recipe not in ("source", "outward_binary_grid"):
        raise ValueError("known declared surface mesh recipe required")
    rows = []
    for way, source in zip(case["ways"], sources, strict=True):
        row = {
            "source_id": source["source_id"],
            "xy": np.array(proposal["axes_xy_m"][source["source_id"]]),
            "height_prior_xyz_m": np.array(way["sampled_design_xyz_m"]),
            "width_m": 2 * source["half_width_m"],
            "gradient": way["max_gradient"],
            "original": {
                "source": {"feature": {"properties": way["tags"]}},
                "bridge": way["derived_bridge"],
            },
        }
        row["prior"] = prior_values(row, row["xy"])
        if mesh_recipe == "outward_binary_grid":
            row["source_grid_m"] = GRID_M
        rows.append(row)
    indices = {r["source_id"]: i for i, r in enumerate(rows)}
    contacts = proposal["connections"]
    nodes = [
        {
            "xy": c["xy_m"],
            "incident": [indices[c["source_id"]], indices[c["target_source_id"]]],
        }
        for c in contacts
    ]
    stairs, directions = [], {}
    for owner, source in enumerate(sources):
        if source["kind"] != "steps":
            continue
        found = {
            c["source_endpoint"]: j
            for j, c in enumerate(contacts)
            if c["source_id"] == source["source_id"]
        }
        if set(found) != {0, -1}:
            raise ValueError(
                "each retained stair needs both explicit construction contacts"
            )
        ascending = rows[owner]["prior"][0] < rows[owner]["prior"][-1]
        stairs.append(
            (
                owner,
                found[0] if ascending else found[-1],
                found[-1] if ascending else found[0],
            )
        )
        directions[owner] = 1 if ascending else -1
    if not stairs:
        raise ValueError("a retained stair recipe is required")
    plan = layout(
        rows, "width", width_reach_m=landing_reach_m, junctions=nodes, stairs=stairs
    )
    plan["stair_directions"] = directions
    plan["construction_contacts"] = contacts
    plan["landing_reach_m"] = landing_reach_m
    plan["mesh_recipe"] = mesh_recipe
    freeze_regions(plan)
    return plan
