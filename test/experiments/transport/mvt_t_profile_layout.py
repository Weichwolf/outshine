"""Adapt retained source parts and the explicit 2D proposals to shared surfaces."""

import copy

import numpy as np
import shapely
from canonical_stair_lofts import bind_lofts, cut_lofts
from mvt_t_stair_loft import prepare_stair_lofts
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


def freeze_regions(plan, source_domains=None):
    """Freeze XY output domains and landing patches before any height solution."""
    domains = [
        shapely.union_all(shapely.polygons(plan["xy"][f])) for f in plan["parts"]
    ]
    footprint_checks = []
    for index, (row, domain) in enumerate(zip(plan["rows"], domains, strict=True)):
        raw = (
            corridor({k: v for k, v in row.items() if k != "source_grid_m"})
            if source_domains is None
            else source_domains[index]
        )
        if source_domains is None and plan["mesh_recipe"] == "outward_binary_grid":
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


def _check_recipes(mesh_recipe, stair_recipe, source_domains):
    if mesh_recipe not in ("source", "outward_binary_grid", "native_interfaces"):
        raise ValueError("known declared surface mesh recipe required")
    if mesh_recipe == "native_interfaces" and source_domains is None:
        raise ValueError("native interfaces require explicitly planned source domains")
    if stair_recipe not in ("station_loft", "legacy_sections"):
        raise ValueError("known constructed flight recipe required")


def _canonical_refiner(rows, stairs, contacts, mesh_recipe, source_domains, scope):
    def refine(xy, faces, owners, nodes, incident, masks):
        geometric = {
            "xy": xy,
            "parts": [faces[owners[:, i]] for i in range(len(rows))],
            "rows": rows,
            "stairs": stairs,
            "nodes": nodes,
            "incident": incident,
            "masks": masks,
            "mesh_recipe": mesh_recipe,
            "construction_contacts": contacts,
        }
        freeze_regions(geometric, source_domains)
        return cut_lofts(geometric, faces, owners, scope)

    return refine


def _profile_row(way, source, proposal, mesh_recipe):
    row = {
        "source_id": source["source_id"],
        "xy": np.array(proposal["axes_xy_m"][source["source_id"]]),
        "height_prior_xyz_m": np.array(way["sampled_design_xyz_m"]),
        "width_m": proposal.get("surface_widths_m", {}).get(
            source["source_id"], 2 * source["half_width_m"]
        ),
        "gradient": way["max_gradient"],
        "original": {
            "source": {"feature": {"properties": way["tags"]}},
            "bridge": way["derived_bridge"],
        },
    }
    row["prior"] = prior_values(row, row["xy"])
    if "surface_axis_constraints" in proposal:
        if type(proposal["surface_axis_constraints"]) is not bool:
            raise ValueError("explicit axis constraint choice required")
        row["surface_axis_constraints"] = proposal["surface_axis_constraints"]
    if mesh_recipe == "outward_binary_grid":
        row["source_grid_m"] = GRID_M
    return row


def prepare_profiles(
    case,
    sources,
    proposal,
    *,
    landing_reach_m=0.75,
    mesh_recipe="source",
    stair_recipe="station_loft",
    source_domains=None,
    section_provider=None,
):
    if not 1 <= len(sources) <= 64 or not 0 <= landing_reach_m <= 2:
        raise ValueError("bounded sources and declared landing reach required")
    _check_recipes(mesh_recipe, stair_recipe, source_domains)
    rows = [
        _profile_row(way, source, proposal, mesh_recipe)
        for way, source in zip(case["ways"], sources, strict=True)
    ]
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
    stage = proposal.get("station_cut_stage", "source_heights")
    scope = proposal.get("station_cut_scope", "global")
    if stage not in ("source_heights", "canonical") or scope not in (
        "global",
        "flight_footprint",
    ):
        raise ValueError("known declared station cut stage and scope required")
    if stage == "canonical" and stair_recipe != "station_loft":
        raise ValueError("canonical cuts require a declared station loft")
    plan = layout(
        rows,
        "width",
        width_reach_m=landing_reach_m,
        junctions=nodes,
        stairs=stairs,
        source_domains=source_domains,
        section_provider=section_provider,
        refine_geometry=_canonical_refiner(
            rows, stairs, contacts, mesh_recipe, source_domains, scope
        )
        if stage == "canonical"
        else None,
    )
    plan["stair_directions"] = directions
    plan["construction_contacts"] = contacts
    plan["landing_reach_m"] = landing_reach_m
    plan["mesh_recipe"] = mesh_recipe
    plan["source_metadata"] = {
        source["source_id"]: copy.deepcopy(source) for source in sources
    }
    plan["coordinate_frame"] = {
        k: case["provenance"][k] for k in ("frame_longitude_latitude_deg", "units")
    }
    if stage == "source_heights":
        freeze_regions(plan, source_domains)
    plan["stair_recipe"] = stair_recipe
    plan["station_cut_scope"] = scope
    plan["station_cut_stage"] = stage
    if stair_recipe == "station_loft":
        if stage == "canonical":
            bind_lofts(plan)
        else:
            prepare_stair_lofts(plan)
    return plan
