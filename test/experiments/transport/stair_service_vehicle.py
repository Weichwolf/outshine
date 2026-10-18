"""Whole centered compact bodies on declared straight/curved one-way service caps."""

import json
from pathlib import Path

import numpy as np
import shapely
from stair_road_vehicle import inspect_curves, lane_directions
from stair_service_profile import ROLE
from transport_vehicle_sweep import straight_body_check


def straight_spans(row, vehicle):
    xy = np.asarray(row["xy"], float)
    axis = np.diff(xy, axis=0)
    length = np.linalg.norm(axis, axis=1)
    directions = axis / length[:, None]
    plan = row.get("horizontal_curve_plan")
    if (
        plan is None
        and len(axis) > 1
        and np.any(np.linalg.norm(np.diff(directions, axis=0), axis=1) > 1e-8)
    ):
        raise ValueError("bent service axis needs an explicit horizontal curve plan")
    corners = {} if plan is None else {c["original_knot"]: c for c in plan["corners"]}
    reverse = lane_directions(row["traffic_profile"]) == [(0, True)]
    start = np.array([corners.get(i, {}).get("trim_m", 0.0) for i in range(len(axis))])
    end = np.array(
        [corners.get(i + 1, {}).get("trim_m", 0.0) for i in range(len(axis))]
    )
    start[0] = (
        max(
            start[0],
            vehicle["frontFromRearAxleM"] if reverse else vehicle["rearOverhangM"],
        )
        + 0.05
    )
    end[-1] = (
        max(
            end[-1],
            vehicle["rearOverhangM"] if reverse else vehicle["frontFromRearAxleM"],
        )
        + 0.05
    )
    if np.any(start + end >= length):
        raise ValueError("service source has no complete interior vehicle interval")
    begin = xy[:-1] + start[:, None] * directions
    finish = xy[1:] - end[:, None] * directions
    heading = np.arctan2(axis[:, 1], axis[:, 0]) + (np.pi if reverse else 0)
    if reverse:
        begin, finish = finish, begin
    return begin, finish, heading, (start[0], end[-1])


def inspect_service(data, rows, *, vehicle=None):
    if vehicle is None:
        vehicle = json.loads(
            Path(__file__).with_name("transport_network_recipes.json").read_text()
        )["vehicleProfiles"]["compact"]
    curves = inspect_curves(data, rows, vehicle=vehicle)
    checks = []
    for i, row in enumerate(rows):
        if row.get("source_recipe") != ROLE:
            continue
        if lane_directions(row["traffic_profile"]) not in ([(0, False)], [(0, True)]):
            raise ValueError("one centered service lane required")
        begin, finish, heading, inset = straight_spans(row, vehicle)
        for cast in (False, True):
            xyz = data["surface_vertices"]
            if cast:
                xyz = xyz.astype("f4").astype(float)
            surface = shapely.union_all(
                shapely.polygons(xyz[data["surface_faces"][data["owners"][:, i]], :2])
            )
            sweeps = []
            for a, b, theta in zip(begin, finish, heading, strict=True):
                proof, _outlines = straight_body_check(surface, a, b, theta, vehicle)
                if not proof["fits_surface"]:
                    raise ValueError("whole centered service vehicle leaves actual cap")
                sweeps.append(dict(proof, begin_xy_m=a.tolist(), end_xy_m=b.tolist()))
            checks.append(
                {
                    "source_id": row["source_id"],
                    "float32": cast,
                    "source_oneway": row["traffic_profile"]["source_oneway"],
                    "lane_offset_m": 0.0,
                    "straight_sweeps": sweeps,
                    "source_end_rear_axle_insets_m": list(inset),
                }
            )
    return {
        "checks": checks,
        "curves": curves,
        "scope": "Whole horizontal rigid compact bodies on the centered supplied one-way lane. Source-end axle intervals remain inset by body extent; no junction route, public access, 3D chassis or dynamics admission.",
    }
