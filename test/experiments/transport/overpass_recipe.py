"""Local two-level strip recipe; original XY/end heights, bounded partial result."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport import plan_vertical_transition, sample_vertical_transition
from overpass_clearance import vertical_clearance
from overpass_ribbon import grade_audit, ribbon, source_frame


def height_capacity(length, grade, second, method):
    if method == "parabolic":
        return (
            second * length**2 / 4
            if length <= 2 * grade / second
            else grade * length - grade**2 / second
        )
    return min(2 * grade * length / 3, second * length**2 / 6)


def fit_upper(frame, endpoints, core, height, grade, method):
    first, last = core
    length = frame["station"][-1]
    tables = []
    reports = []
    for z0, z1, run, shift in (
        (endpoints[0], height, first, 0.0),
        (height, endpoints[1], length - last, last),
    ):
        if run < 1e-6:
            return None, {"reason": "no_approach_space"}
        curve, report = plan_vertical_transition(
            z0, z1, run, grade=grade, method=method
        )
        if curve is None:
            return None, report
        table, sampling = sample_vertical_transition(curve, max_vertices=2048)
        if table is None:
            return None, sampling
        table[:, 0] += shift
        tables.append(table)
        reports.append(dict(report, sampling=sampling))
    table = np.vstack((tables[0], tables[1]))
    stations = np.unique(np.r_[table[:, 0], frame["station"]])
    if len(stations) > 4096:
        return None, {"reason": "combined_profile_vertex_budget"}
    # Added corner stations split existing chords, preserving their certified error.
    values = np.interp(stations, table[:, 0], table[:, 1])
    return np.c_[stations, values], {"approaches": reports}


def validate_limits(case, method, clearance, depth, margin):
    if (
        method not in ("parabolic", "cubic")
        or not all(np.isfinite(x) for x in (clearance, depth, margin))
        or not 0.1 <= clearance <= 20
        or not 0.001 <= depth <= 2
        or not 0 <= margin <= 50
    ):
        raise ValueError("known profile and finite physical recipe limits required")

    for source in (case["upper"], case["lower"]):
        if (
            not np.isfinite(source["grade"])
            or not 0 < source["grade"] <= 1
            or not np.isfinite(source["crossfall"])
            or abs(source["crossfall"]) > 0.025
        ):
            raise ValueError("finite class grade and bounded crossfall required")


def build(case, *, method="parabolic", clearance=4.5, depth=0.35, margin=2.0):
    report = {
        "valid": False,
        "reason": "",
        "method": method,
        "upper_source": case["upper"]["source_id"],
        "lower_source": case["lower"]["source_id"],
        "requested_clearance_m": clearance,
        "depth_m": depth,
    }
    try:
        validate_limits(case, method, clearance, depth, margin)
        upper, lower = case["upper"], case["lower"]
        if lower.get("properties", {}).get("subclass") == "steps":
            return None, dict(report, reason="unsupported_stair_surface")
        upper_layer = upper.get("properties", {}).get("layer", 1)
        lower_layer = lower.get("properties", {}).get("layer", 0)
        if (
            type(upper_layer) is not int
            or type(lower_layer) is not int
            or upper_layer <= lower_layer
        ):
            return None, dict(report, reason="ambiguous_level_order")
        report["level_order"] = [lower_layer, upper_layer]
        uf, lf = (
            source_frame(upper["xy"], upper["width_m"]),
            source_frame(lower["xy"], lower["width_m"]),
        )
        lower_table = np.c_[
            lf["station"],
            np.interp(lf["station"], [0.0, lf["station"][-1]], lower["end_heights_m"]),
        ]
        lower_mesh = ribbon(lf, lower_table, lower["width_m"], lower["crossfall"])
        lower_check = grade_audit(lf, lower_mesh, lower_table, lower["grade"])
        overlap = uf["footprint"].intersection(lf["footprint"])
        if overlap.area <= 0:
            return None, dict(report, reason="no_positive_area_overlap")
        coordinates = shapely.get_coordinates(overlap)
        stations = shapely.line_locate_point(
            shapely.LineString(uf["xy"]), shapely.points(coordinates)
        )
        core = [
            max(0.0, float(stations.min()) - margin),
            min(float(uf["station"][-1]), float(stations.max()) + margin),
        ]
        flat = ribbon(
            uf,
            np.c_[uf["station"], np.zeros(len(uf["station"]))],
            upper["width_m"],
            0.0,
        )
        lower_bound = vertical_clearance(lower_mesh, flat)
        if not lower_bound["valid"]:
            return None, dict(report, reason=lower_bound["reason"])
        height = (
            -lower_bound["minimum_clearance_m"]
            + clearance
            + depth
            + abs(upper["crossfall"]) * upper["width_m"] / 2
            + 0.02
        )
        effective = upper["grade"] * 0.8
        runs = np.array([core[0], uf["station"][-1] - core[1]])
        reachable = np.array(
            [height_capacity(run, effective, 0.002, method) for run in runs]
        )
        endpoints = np.asarray(upper["end_heights_m"])
        report.update(
            core_station_m=core,
            required_deck_height_m=height,
            end_heights_m=endpoints.tolist(),
            approach_lengths_m=runs.tolist(),
            maximum_reachable_rise_m=reachable.tolist(),
            lower=lower_check,
        )
        if np.any(abs(height - endpoints) > reachable + 1e-7):
            return None, dict(report, reason="insufficient_approach_length")
        table, profiles = fit_upper(uf, endpoints, core, height, effective, method)
        if table is None:
            return None, dict(report, reason=profiles["reason"])
        top = ribbon(uf, table, upper["width_m"], upper["crossfall"])
        upper_check = grade_audit(uf, top, table, upper["grade"])
        source_stations = np.repeat(table[:, 0], 2)[top.triangles]
        rigid = np.all(
            (source_stations >= core[0]) & (source_stations <= core[1]), axis=1
        )
        rigid_footprint = shapely.coverage_union_all(
            shapely.polygons(top.vertices[top.triangles[rigid], :2])
        )
        if overlap.difference(rigid_footprint).area > 1e-7:
            return None, dict(report, reason="incomplete_rigid_overlap_core")
        bottom = Mesh(top.vertices - [0.0, 0.0, depth], top.triangles)
        gap = vertical_clearance(lower_mesh, bottom)
        if not gap["valid"] or gap["minimum_clearance_m"] < clearance - 1e-7:
            return None, dict(
                report,
                reason=gap["reason"] or "insufficient_full_surface_clearance",
                clearance=gap,
            )
        report.update(
            valid=True,
            reason="",
            upper=upper_check,
            profiles=profiles,
            clearance=gap,
            rigid_overlap_core_covers=True,
            overlap_area_m2=float(overlap.area),
            vertices=len(top.vertices) + len(lower_mesh.vertices),
            triangles=len(top.triangles) + len(lower_mesh.triangles),
        )
        return (top, lower_mesh), report
    except ValueError as error:
        return None, dict(report, reason="invalid_local_recipe", detail=str(error))
