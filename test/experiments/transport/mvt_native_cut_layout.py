"""Three-source local proposals constrained by real native mesh cross sections."""

import copy
from collections import defaultdict

import numpy as np
import shapely
from mvt_native_cut_domains import cut_ribbon
from mvt_native_cut_profiles import cut_profiles
from mvt_native_path_sections import sections
from mvt_t_profile_layout import prepare_profiles
from shared_bridge_geometry import corridor


def prepare_native_cut_plan(
    case, sources, proposal, native_case, *, landing_reach_m=0.3, section_spacing_m=None
):
    for key in ("frame_longitude_latitude_deg", "units"):
        if case["provenance"][key] != native_case["provenance"][key]:
            raise ValueError(
                "native cuts and proposed sources need one coordinate frame"
            )
    proposal = copy.deepcopy(proposal)
    profiles = cut_profiles(native_case)
    grouped = defaultdict(list)
    for profile in profiles:
        grouped[profile["source_id"]].append(profile)
    if not grouped.keys() <= {source["source_id"] for source in sources}:
        raise ValueError("native cuts must belong to retained source parts")
    domains, scopes = [], {}
    for source in sources:
        key = source["source_id"]
        if key in grouped:
            ports = sorted(grouped[key], key=lambda p: p["source_station_m"])
            if len(ports) != 2:
                raise ValueError("two actual interfaces per cropped source required")
            if any(
                not np.array_equal(p["original_source_xy_m"], source["xy_m"])
                for p in ports
            ):
                raise ValueError(
                    "native cut stations must refer to the retained original axis"
                )
            displacement = shapely.LineString(source["xy_m"]).hausdorff_distance(
                shapely.LineString(proposal["axes_xy_m"][key])
            )
            if displacement > 1e-10:
                raise ValueError(
                    "cropping currently requires the unchanged target axis"
                )
            xy, domain, scope = cut_ribbon(source["xy_m"], *ports)
            scope["numeric_proposal_axis_displacement_m"] = displacement
            proposal["axes_xy_m"][key] = xy.tolist()
            proposal.setdefault("surface_widths_m", {})[key] = max(
                p["raw_section"]["width_m"] for p in ports
            )
            scopes[key] = scope
        else:
            domain = corridor(
                {
                    "xy": np.array(proposal["axes_xy_m"][key]),
                    "width_m": 2 * source["half_width_m"],
                }
            )
            scopes[key] = {"scope": "Complete constructed stair axis retained."}
        domains.append(domain)
    by_source = {
        source["source_id"]: domain
        for source, domain in zip(sources, domains, strict=True)
    }
    plan = prepare_profiles(
        case,
        sources,
        proposal,
        landing_reach_m=landing_reach_m,
        mesh_recipe="native_interfaces",
        source_domains=domains,
        section_provider=lambda row: sections(
            row, by_source[row["source_id"]], section_spacing_m
        ),
    )
    contacts = []
    for profile in profiles:
        owner = next(
            i
            for i, r in enumerate(plan["rows"])
            if r["source_id"] == profile["source_id"]
        )
        actual = {tuple(p) for p in plan["xy"][np.unique(plan["parts"][owner])]}
        if any(tuple(p[:2]) not in actual for p in profile["profile_xyz_m"]):
            raise ValueError(
                "immutable native interface vertex was lost during preparation"
            )
        contacts.append(
            {
                "contact_id": f"native-cut:{profile['source_id']}:{profile['source_station_m']}",
                "source_id": profile["source_id"],
                "binding": "fixed",
                "xyz_m": profile["profile_xyz_m"],
                "provenance": f"Actual native mesh cut from {native_case['provenance']['native_checkout_commit']}; original source station is not a qualified native navstation.",
            }
        )
    plan["native_cut_context"] = {
        "profiles": profiles,
        "source_scopes": scopes,
        "original_case": copy.deepcopy(case),
        "native_provenance": copy.deepcopy(native_case["provenance"]),
        "scope": "Bounded proposed replacement; unchanged outside triangle/nav/terrain integration is not yet qualified.",
    }
    return plan, contacts
