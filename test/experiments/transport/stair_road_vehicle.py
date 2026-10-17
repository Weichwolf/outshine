"""Whole horizontal compact-car bodies on actual curved caps and explicit lanes."""

import json
from pathlib import Path

import numpy as np
import shapely
from transport_clothoid_turns import fit_symmetric_turns, sample_symmetric_turns
from transport_vehicle_sweep import swept_hull_check


def lane_directions(profile):
    if profile["forward_lanes"] == 2:
        return [(1, False), (-1, False)]
    if profile["reverse_lanes"] == 2:
        return [(1, True), (-1, True)]
    return [(-1, False), (1, True)]


def curve_sweep(surface, row, note, side, reverse, vehicle):
    recipe = row["horizontal_curve_plan"]["recipe"]
    angle, heading = note["turn_rad"], note["incoming_heading_rad"]
    fit = fit_symmetric_turns(
        row["xy"][note["original_knot"]],
        np.array([note["trim_m"]]),
        np.array([heading]),
        np.array([heading + angle]),
        transition_fraction=0.25,
    )
    length = float(fit["length_m"][0])
    curvature = float(fit["maximum_curvature_per_m"][0])
    rate = float(fit["maximum_curvature_rate_per_m2"][0])
    if recipe["model"] == "circle":
        length = abs(angle) * recipe["radius_m"]
        curvature, rate = 1 / recipe["radius_m"], 0.0
    offset = side * row["width_m"] / 4
    radius = np.hypot(
        max(vehicle["frontFromRearAxleM"], vehicle["rearOverhangM"]),
        abs(offset) + vehicle["widthM"] / 2,
    )
    minimum_radius = 1 / curvature - abs(offset)
    if minimum_radius < vehicle["minimumRearAxleRadiusM"]:
        raise ValueError("whole lane curve violates compact rear-axle radius")

    def evaluate(_owners, stations):
        if recipe["model"] == "clothoid":
            points, angles, _k = sample_symmetric_turns(fit, stations)
            position, theta = points[0], angles[0]
        else:
            theta = heading + angle * stations
            normal = np.c_[-np.sin(theta), np.cos(theta)]
            start = row["xy"][note["original_knot"]] - note["trim_m"] * np.array(
                [np.cos(heading), np.sin(heading)]
            )
            center = start + recipe["radius_m"] * np.sign(angle) * np.array(
                [-np.sin(heading), np.cos(heading)]
            )
            position = center - recipe["radius_m"] * np.sign(angle) * normal
        normal = np.c_[-np.sin(theta), np.cos(theta)]
        return position + offset * normal, theta + (np.pi if reverse else 0)

    acceleration = length**2 * (curvature + radius * (rate + curvature**2))
    proof = swept_hull_check(surface, evaluate, [acceleration], vehicle, error=0.025)
    if not proof["fits_surface"]:
        raise ValueError("whole compact vehicle leaves actual road lane: " + str(proof))
    return dict(
        proof,
        minimum_rear_axle_radius_m=minimum_radius,
        side=side,
        reverse=reverse,
        original_knot=note["original_knot"],
    )


def inspect_curves(data, rows):
    vehicle = json.loads(
        Path(__file__).with_name("transport_network_recipes.json").read_text()
    )["vehicleProfiles"]["compact"]
    proof = []
    for i, row in enumerate(rows):
        plan = row.get("horizontal_curve_plan")
        if plan is None or not plan["corners"]:
            continue
        axis = shapely.LineString(row["surface_axis_xy_m"])
        for cast in (False, True):
            xyz = data["surface_vertices"]
            if cast:
                xyz = xyz.astype("f4").astype(float)
            cap = shapely.union_all(
                shapely.polygons(xyz[data["surface_faces"][data["owners"][:, i]], :2])
            )
            for side, reverse in lane_directions(row["traffic_profile"]):
                lane = axis.buffer(
                    side * row["width_m"] / 2,
                    single_sided=True,
                    cap_style="flat",
                    join_style="mitre",
                    mitre_limit=2,
                )
                actual = cap.intersection(lane)
                for note in plan["corners"]:
                    check = curve_sweep(actual, row, note, side, reverse, vehicle)
                    proof.append(dict(check, source_id=row["source_id"], float32=cast))
    return {
        "checks": proof,
        "vehicle": vehicle,
        "scope": "Whole horizontal rigid compact body within actual source cap and procedural right-hand lane around each internal curve. No junction/endpoint route, 3D chassis, dynamic steering or traffic-rule acceptance.",
    }
