"""A bounded native-frontier body proposal with explicit interior mesh choices."""

import copy

import numpy as np
import shapely
from mvt_native_cut_layout import prepare_native_cut_plan


def _navigation_axes(plan, proposal):
    for row in plan["rows"]:
        key = row["source_id"]
        scope = plan["native_cut_context"]["source_scopes"][key]
        if "source_station_range_m" not in scope:
            continue
        original = plan["source_metadata"][key]["xy_m"]
        line = shapely.LineString(original)
        a, b = scope["source_station_range_m"]
        points = np.array(proposal["axes_xy_m"][key])
        station = shapely.line_locate_point(line, shapely.points(points))
        inside = points[(station > a) & (station < b)]
        row["navigation_axis_xy_m"] = np.vstack([row["xy"][0], inside, row["xy"][-1]])


def prepare_native_body_plan(
    case,
    sources,
    proposal,
    native_case,
    *,
    section_spacing_m=2.0,
    tread_partition_recipe="envelope_indexed_band_remesh",
):
    if tread_partition_recipe not in (
        "indexed_band_remesh",
        "envelope_indexed_band_remesh",
    ):
        raise ValueError("known declared native body partition recipe required")
    proposal = copy.deepcopy(proposal)
    for source in sources:
        if source["kind"] != "steps":
            continue
        xy = np.array(proposal["axes_xy_m"][source["source_id"]])
        direction = xy[-1] - xy[0]
        length = np.linalg.norm(direction)
        if length == 0:
            raise ValueError("native body proposal needs a positive straight flight")
        tangent = direction / length
        normal = np.array([-tangent[1], tangent[0]])
        if abs((xy - xy[0]) @ normal).max() > 1e-8:
            raise ValueError("native body proposal cannot straighten a curved flight")
        proposal["axes_xy_m"][source["source_id"]] = [xy[0].tolist(), xy[-1].tolist()]
    proposal["surface_axis_constraints"] = False
    plan, contacts = prepare_native_cut_plan(
        case,
        sources,
        proposal,
        native_case,
        landing_reach_m=0,
        section_spacing_m=section_spacing_m,
    )
    _navigation_axes(plan, proposal)
    plan["tread_partition_recipe"] = tread_partition_recipe
    plan["consumer_sampling_tolerance_m"] = 1e-12
    plan["native_cut_context"]["body_recipe"] = {
        "interior_path_section_spacing_m": section_spacing_m,
        "surface_axis_constraints": False,
        "tread_partition_recipe": tread_partition_recipe,
        "consumer_sampling_tolerance_m": 1e-12,
        "sampling_scope": "Numeric affine lookup only for axes/nodes inside the unsimplified prepared domains. Native fixed-contact audits retain zero XY tolerance.",
        "scope": "Local straight-flight construction. Original native navstation and outside mesh/terrain integration remain unqualified.",
    }
    return plan, contacts
