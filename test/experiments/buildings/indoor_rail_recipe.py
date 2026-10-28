"""Plausible planar handrail paths from chosen stairs, with traffic clipping."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
from indoor_rail_clearance import RailClearance
from indoor_rail_geometry import hollow_sweep


@dataclass(frozen=True)
class RailLimits:
    width_m: float = 0.04
    wall_m: float = 0.002
    handrail_height_m: float = 1.0
    transition_m: float = 0.3
    clearance_m: float = 0.002
    offset_margin_m: float = 0.001
    minimum_piece_m: float = 0.15
    max_segments: int = 1024

    def __post_init__(self):
        values = (
            self.width_m,
            self.wall_m,
            self.handrail_height_m,
            self.transition_m,
            self.clearance_m,
            self.offset_margin_m,
            self.minimum_piece_m,
        )
        if (
            not np.isfinite(values).all()
            or min(values) <= 0
            or 2 * self.wall_m >= self.width_m
        ):
            raise ValueError("Positive finite rail recipe and hollow section required")
        if type(self.max_segments) is not int or not 2 <= self.max_segments <= 4096:
            raise ValueError("Bounded integer rail path capacity required")


def handrail_profile(stair, limits):
    recipe = stair["recipe"]
    length = stair["surfaces"][-1]["end_m"]
    low, high = (e["z_m"] for e in stair["end_profiles"])
    levels = np.linspace(0, high - low, recipe["risers"] + 1)
    points = [(0.0, limits.handrail_height_m)]
    done = 0
    # Existing actual landing boundaries avoid recovering semantic labels or
    # assuming a nominal storey height in this consumer.
    landings = [
        stair["surfaces"][int(i)] for i in np.r_[0, np.cumsum(recipe["flight_risers"])]
    ]
    for flight, count in enumerate(recipe["flight_risers"]):
        start = landings[flight]["end_m"]
        end = start + (count - 1) * recipe["tread_m"]
        points.extend(
            (
                (
                    start
                    - min(
                        limits.transition_m,
                        (landings[flight]["end_m"] - landings[flight]["start_m"]) / 2,
                    ),
                    float(levels[done]) + limits.handrail_height_m,
                ),
                (start, float(levels[done + 1]) + limits.handrail_height_m),
                (end, float(levels[done + count]) + limits.handrail_height_m),
            )
        )
        done += count
    points.append((length, high - low + limits.handrail_height_m))
    points = np.asarray(points)
    return points[np.r_[True, np.any(np.diff(points, axis=0) != 0, axis=1)]]


def stair_pose(stair):
    corners = np.asarray(stair["surfaces"][0]["polygon"].exterior.coords)
    origin = np.r_[(corners[0] + corners[3]) / 2, stair["end_profiles"][0]["z_m"]]
    tangent = (corners[1] - corners[0]) / np.linalg.norm(corners[1] - corners[0])
    normal = np.array([-tangent[1], tangent[0]])
    basis = np.column_stack((np.r_[tangent, 0.0], (0, 0, 1), np.r_[normal, 0.0]))
    return origin, basis


def handrails(batch, floor_reservations, *, limits=None):
    limits = RailLimits() if limits is None else limits
    reservations = (*floor_reservations, *batch["reservations"])
    # A ninety-degree planar miter has radial extent width, including both
    # cross-section dimensions; the prepared keepout additionally has margin.
    index = RailClearance(reservations, limits.width_m, limits.clearance_m)
    result, ledger = [], []
    for stair in batch["stairs"]:
        profile = handrail_profile(stair, limits)
        origin, basis = stair_pose(stair)
        half = stair["mesh_audit"]["constructed_width_m"] / 2
        offset = half + index.radius_xy + limits.offset_margin_m
        records = []
        reason = ""
        for side in (-1, 1):
            anchor = origin + basis[:, 2] * side * offset
            world = np.c_[profile, np.zeros(len(profile))] @ basis.T + anchor
            for index_segment, (a, b) in enumerate(pairwise(profile)):
                if b[1] > a[1] and index.blocked(
                    world[index_segment], world[index_segment + 1]
                ):
                    reason = "required_handrail_flight_clipped"
                    break
            if reason:
                break
            pieces = index.clip(world)
            for piece in pieces:
                local = (piece - anchor) @ basis
                if (
                    np.linalg.norm(np.diff(piece, axis=0), axis=1).sum()
                    < limits.minimum_piece_m
                ):
                    continue
                if len(piece) > limits.max_segments:
                    raise ValueError("rail_station_capacity")
                try:
                    mesh, audit = hollow_sweep(
                        local[:, :2], limits.width_m, limits.wall_m, basis, anchor
                    )
                except ValueError as error:
                    reason = str(error)
                    break
                records.append(
                    {
                        "source_id": stair["source_id"],
                        "source_ids": stair["source_ids"],
                        "side": side,
                        "world_path": piece,
                        "profile": local[:, :2],
                        "basis": basis,
                        "origin": anchor,
                        "mesh": mesh,
                        "mesh_audit": audit,
                    }
                )
            if reason:
                break
        if reason:
            ledger.append(
                {"source_id": stair["source_id"], "status": "pending", "reason": reason}
            )
        else:
            result.extend(records)
            ledger.append(
                {"source_id": stair["source_id"], "status": "candidate", "reason": ""}
            )
    return {
        "handrails": tuple(result),
        "ledger": ledger,
        "traffic_tests": index.tests,
        "scope": "Candidate continuous hollow handrails only; posts/anchors, deck guards, final traffic/material/source checks and qualification still required.",
    }
