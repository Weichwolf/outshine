"""Bounded local source-driven proposals with whole actual side and mouth contacts."""

import numpy as np
import shapely
from foot_ramp_geometry import FLARE_M, LANDING_END_M, RETURN_M, outline


def placement(foot, road, side):
    f, r = foot["xy"], road["xy"]
    ft = (f[1] - f[0]) / np.linalg.norm(f[1] - f[0])
    rt = (r[1] - r[0]) / np.linalg.norm(r[1] - r[0])
    if abs(ft @ rt) > 0.03:
        raise ValueError("ramp_source_not_nearly_orthogonal")
    half = min(2.0, foot["half"] * 0.8)
    if half < 0.3:
        raise ValueError("ramp_source_width_too_small")
    alpha, _beta = np.linalg.solve(np.column_stack((rt, -ft)), f[0] - r[0])
    crossing = r[0] + alpha * rt
    if road["line"].distance(shapely.Point(crossing)) > 0.25:
        raise ValueError("ramp_not_at_actual_urban_edge")
    y = side * np.array([-rt[1], rt[0]])
    basis = np.column_stack((side * rt, y))
    origin = crossing + y * road["half"]
    shape = outline(origin, basis, half)
    axis = shapely.LineString([origin, origin + y * (LANDING_END_M + RETURN_M)])
    if not foot["line"].buffer(half / 2, cap_style="flat").covers(axis):
        raise ValueError("ramp_axis_not_supported_by_actual_foot_source")
    return origin, basis, half, shape


def candidate(
    foot, road, side, old, traffic, world, obstacles, field, tree, triangles, outside
):
    origin, basis, half, shape = placement(foot, road, side)
    width = half + FLARE_M
    sides = [
        shapely.LineString(
            np.array([[x, -0.01], [x, LANDING_END_M]]) @ basis.T + origin
        )
        for x in [-width, width]
    ]
    if any(s.intersection(old).length < 1.8 for s in sides):
        raise ValueError("ramp_missing_actual_whole_side_contact")
    window = shapely.Polygon(
        np.array([[-width, 1.8], [width, 1.8], [width, 4.5], [-width, 4.5]]) @ basis.T
        + origin
    )
    contacts = shapely.get_coordinates(old.intersection(window))
    end = max(LANDING_END_M, float(np.max(((contacts - origin) @ basis)[:, 1])) + 0.05)
    if end > 4.5:
        raise ValueError("ramp_actual_outer_landing_length_budget")
    shape = outline(origin, basis, half, end)
    axis = shapely.LineString([origin, origin + basis[:, 1] * (end + RETURN_M)])
    if not foot["line"].buffer(half / 2, cap_style="flat").covers(axis):
        raise ValueError("ramp_actual_outer_landing_not_source_supported")
    mouths = [
        shapely.LineString(np.array([[-half, t], [half, t]]) @ basis.T + origin)
        for t in [0, end + RETURN_M]
    ]
    if any(s.difference(traffic).length > 1e-7 for s in mouths):
        raise ValueError("ramp_missing_actual_traffic_mouth")
    back = shapely.Polygon(
        np.array(
            [
                [-width, end],
                [width, end],
                [half, end + RETURN_M],
                [-half, end + RETURN_M],
            ]
        )
        @ basis.T
        + origin
    )
    if back.intersection(old).area > 1e-8:
        raise ValueError("ramp_return_conflicts_with_existing_raised_surface")
    corridor = foot["line"].buffer(foot["half"] + 0.01, cap_style="flat")
    shape = shape.intersection(shapely.union_all([old, outside, corridor]))
    if (
        shape.geom_type != "Polygon"
        or not shape.is_valid
        or shapely.get_num_coordinates(shape) > 1024
    ):
        raise ValueError("ramp_actual_profile_boundary_not_a_bounded_polygon")
    if shape.difference(world).area > 1e-8 or shape.intersection(obstacles).area > 1e-8:
        raise ValueError("ramp_outside_world_or_protected_structure")
    hits = tree.query(shape, predicate="intersects")
    selected = [int(i) for i in hits if shape.difference(triangles[i]).area <= 1e-8]
    if len(selected) != 1:
        raise ValueError("ramp_not_on_one_explicit_affine_ground_patch")
    index = selected[0]
    return {
        "origin": origin.tolist(),
        "basis": basis.tolist(),
        "half": half,
        "shape": shape,
        "field_face": index,
        "triangle": field.vertices[field.triangles[index]].tolist(),
        "foot_sources": foot["sources"],
        "road_sources": road["sources"],
        "access_basis": foot["access_basis"],
        "side": side,
        "landing_end_m": end,
        "source_corridor_half_m": foot["half"] + 0.01,
    }


def plan(feet, roads, old, traffic, world, obstacles, field):
    road_tree = shapely.STRtree([r["line"] for r in roads])
    triangles = shapely.polygons(field.vertices[field.triangles, :2])
    field_tree = shapely.STRtree(triangles)
    outside = world.difference(traffic)
    chosen, ledger = [], []
    tried = 0
    for foot in feet:
        if tried >= 20000 or len(chosen) >= 64:
            ledger.append(
                {
                    "foot_sources": foot["sources"],
                    "reason": "ramp_plan_pair_or_candidate_budget",
                }
            )
            continue
        attempts = 0
        for index in road_tree.query(foot["line"].buffer(20), predicate="intersects"):
            road = roads[index]
            for side in [-1, 1]:
                if tried >= 20000 or len(chosen) >= 64:
                    ledger.append(
                        {
                            "foot_sources": foot["sources"],
                            "reason": "ramp_plan_pair_or_candidate_budget",
                        }
                    )
                    break
                tried += 1
                attempts += 1
                try:
                    row = candidate(
                        foot,
                        road,
                        side,
                        old,
                        traffic,
                        world,
                        obstacles,
                        field,
                        field_tree,
                        triangles,
                        outside,
                    )
                except ValueError as error:
                    ledger.append(
                        {
                            "foot_sources": foot["sources"],
                            "road_sources": road["sources"],
                            "side": side,
                            "reason": str(error),
                        }
                    )
                else:
                    if any(
                        row["shape"].intersection(other["shape"]).area > 1e-8
                        for other in chosen
                    ):
                        ledger.append(
                            {
                                "foot_sources": foot["sources"],
                                "road_sources": road["sources"],
                                "side": side,
                                "reason": "ramp_duplicate_or_overlapping_proposal",
                            }
                        )
                    else:
                        row["candidate"] = len(chosen)
                        chosen.append(row)
            if tried >= 20000 or len(chosen) >= 64:
                break
        if not attempts:
            ledger.append(
                {
                    "foot_sources": foot["sources"],
                    "reason": "ramp_no_nearby_eligible_urban_edge",
                }
            )
    return chosen, ledger, {"pair_attempts": tried, "candidates": len(chosen)}
