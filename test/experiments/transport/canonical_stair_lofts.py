"""Select canonical flight facets and bind their stations to later height owners."""

import numpy as np
import shapely
from canonical_station_cuts import refine
from mvt_t_stair_loft import _flight


def cut_lofts(plan, faces, owners, scope):
    flights = [_flight(plan, owner) for owner, _low, _high in plan["stairs"]]
    axes = [
        {
            "origin_xy_m": flight["origin_xy_m"].tolist(),
            "tangent": flight["tangent"].tolist(),
            "cuts_m": [flight["start_cut_m"], flight["end_cut_m"]],
            "initial_faces": np.flatnonzero(owners[:, flight["owner"]]).tolist()
            if scope == "flight_footprint"
            else None,
        }
        for flight in flights
    ]
    bits = np.left_shift(np.uint64(1), np.arange(owners.shape[1], dtype=np.uint64))
    result = refine(plan["xy"], faces, owners.astype(np.uint64) @ bits, axes)
    refined_owners = (result["source_masks"][:, None] & bits) != 0
    errors = []
    for owner, domain in enumerate(plan["prepared_domains"]):
        cells = shapely.polygons(
            result["xy_m"][result["triangles"][refined_owners[:, owner]]]
        )
        error = float(shapely.union_all(cells).symmetric_difference(domain).area)
        if not shapely.coverage_is_valid(cells) or error > 1e-7:
            raise ValueError("canonical station cuts changed prepared source coverage")
        errors.append(error)
    stats = dict(result["work"])
    stats.update(
        cut_stage="canonical",
        cut_scope=scope,
        planar_faces=len(result["triangles"]),
        maximum_extra_flat_m=max(max(f["extra_flat_m"]) for f in flights),
        source_coverage_errors_m2=errors,
    )
    regions = {
        key: plan[key]
        for key in (
            "prepared_domains",
            "input_landing_masks",
            "masks",
            "footprint_checks",
            "landing_displacement_m",
        )
    }
    return (
        result["xy_m"],
        result["triangles"],
        refined_owners,
        {
            "canonical_station_mesh": result,
            "canonical_stair_lofts": flights,
            "canonical_regions": regions,
            "station_refinement": stats,
        },
    )


def bind_lofts(plan):
    mesh = plan["canonical_station_mesh"]
    flights = plan.pop("canonical_stair_lofts")
    for axis, flight in enumerate(flights):
        flight["stations_m"] = mesh["stations_m"][axis][
            plan["height_planar_vertices"]
        ].tolist()
    plan["stair_lofts"] = flights
    plan.update(plan.pop("canonical_regions"))
