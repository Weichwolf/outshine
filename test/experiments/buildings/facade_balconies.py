"""Source-stable bounded residential balcony cells; no second facade or room grid."""

import numpy as np
import shapely
from outshine_ref.buildings.balconies import (
    BalconyPlan,
    balcony_plates,
    balcony_surface,
)
from outshine_ref.buildings.interiors.geometry import rectangle


def cell(entry, column, level, states):
    frame = entry["frame"]
    tangent = np.asarray(entry["door"]["tangent"])
    centre = np.asarray(frame["a"]) + (column + 0.5) * entry["cell_width_m"] * tangent
    identifier = (
        entry["id"].removesuffix(":floor-0:entrance")
        + f":floor-{level}:balcony-{column}"
    )
    door_id = identifier + ":portal"
    angle = states.get(door_id, 0.0)
    if isinstance(angle, (bool, str)) or not np.isfinite(angle) or not 0 <= angle <= 90:
        raise ValueError("Balcony door angle outside[0,90]")
    prototype = BalconyPlan(min(3.2, entry["cell_width_m"] * 0.9))
    footprint = rectangle(centre, tangent, prototype.width_m, prototype.depth_m, 0.0)
    pocket = rectangle(centre, tangent, 1.4, 0.0, 1.84)
    clear_pocket = rectangle(centre, tangent, 1.4, -0.25, 1.84)
    base = level * entry["floor_m"]
    doorway = entry | {
        "id": door_id,
        "column": column,
        "angle_deg": angle,
        "door": {
            "centre": centre.tolist(),
            "tangent": tangent.tolist(),
            "height_m": 2.15,
        },
    }
    return {
        "id": identifier,
        "door_id": door_id,
        "column": column,
        "row": level,
        "base_m": base,
        "floor_z_m": base + 0.16,
        "prototype": prototype,
        "footprint": footprint,
        "pocket": pocket,
        "clear_pocket": clear_pocket,
        "doorway": doorway,
    }


class BalconyDetails:
    def __init__(self, entrances, states=None):
        self.entrances, self.states = entrances, states or {}
        self.planned, self.rejected, self.templates = [], [], {}

    def template(self, plan, *, plates=False):
        key = plan, plates
        if key not in self.templates:
            self.templates[key] = (
                balcony_plates(plan) if plates else balcony_surface(plan)
            )
        return self.templates[key]

    def plan(self, row, entry, family):
        if family != "residential" or entry is None or row.get("clipped"):
            return None
        frame = entry["frame"]
        if (
            frame["rows"] < 2
            or entry["cell_width_m"] < 2.0
            or not 60 <= row["polygon"].area <= 1600
        ):
            return None
        columns = (
            sorted({round(t * (frame["columns"] - 1)) for t in (0.25, 0.75)})
            if frame["columns"] >= 6
            else [entry["column"]]
        )
        planned = []
        for column in columns:
            for level in range(1, min(frame["rows"], 4)):
                try:
                    current = cell(entry, column, level, self.states)
                    low, high = (
                        current["base_m"],
                        current["floor_z_m"] + current["prototype"].guard_height_m,
                    )
                    if current["footprint"].intersection(row["polygon"]).area > 1e-8:
                        raise ValueError(
                            "Balcony intersects another wing of its source"
                        )
                    wall = rectangle(
                        current["doorway"]["door"]["centre"],
                        current["doorway"]["door"]["tangent"],
                        entry["cell_width_m"],
                        0.0,
                        0.24,
                    )
                    if (
                        wall.difference(row["polygon"]).area > 1e-8
                        or current["pocket"].difference(row["polygon"]).area > 1e-8
                        or not row["polygon"]
                        .buffer(-0.24, join_style=2)
                        .covers(current["clear_pocket"])
                    ):
                        raise ValueError(
                            "Balcony wall cell or arrival pocket leaves its source"
                        )
                    # Reserve room for an opposing procedural deck (all decks project at most1.2m).
                    envelope = current["footprint"].buffer(1.2)
                    if not self.entrances.clear(
                        row, envelope, high, low_m=low
                    ) or not self.entrances.clear(
                        row, current["pocket"], current["floor_z_m"] + 2.15, low_m=low
                    ):
                        raise ValueError(
                            "Balcony conflicts with a known height interval or opposing-deck margin"
                        )
                    planned.append(current)
                except ValueError as error:
                    self.rejected.append(
                        {
                            "source_id": row["id"],
                            "cell": [column, level],
                            "reason": str(error),
                        }
                    )
        if not planned:
            return None
        self.planned.append(
            {
                "source_id": row["id"],
                "facade_id": frame["id"],
                "origin": "procedural residential facade interpretation, not supplied balcony geometry",
                "cells": [
                    {
                        "id": c["id"],
                        "door_id": c["door_id"],
                        "cell": [c["column"], c["row"]],
                        "floor_z_m": c["floor_z_m"],
                        "width_m": c["prototype"].width_m,
                        "depth_m": c["prototype"].depth_m,
                        "outside_footprint_wkb": c["footprint"].wkb_hex,
                        "height_interval_m": [
                            c["base_m"],
                            c["floor_z_m"] + c["prototype"].guard_height_m,
                        ],
                    }
                    for c in planned
                ],
                "scope": "Bounded local balcony arrival; no full upper-storey, transport/vegetation or native admission",
            }
        )
        return {"frame": frame, "entry": entry, "cells": planned, "owner": self}


def demand(current, view):
    door, plan = current["doorway"]["door"], current["prototype"]
    centre, tangent = np.asarray(door["centre"]), np.asarray(door["tangent"])
    outward = np.array([tangent[1], -tangent[0]])
    eye_z = view.eye[2] - current["floor_z_m"]
    if 0 <= eye_z <= current["doorway"]["floor_m"] - 0.18 and (
        current["footprint"].covers(shapely.Point(view.eye[:2]))
        or current["pocket"].covers(shapely.Point(view.eye[:2]))
    ):
        return "through"
    points = np.array(
        [
            np.r_[centre + tangent * x + outward * y, current["floor_z_m"] + z]
            for x in (-plan.width_m / 2, plan.width_m / 2)
            for y in (-plan.back_m, plan.depth_m)
            for z in (-plan.slab_m, plan.guard_height_m)
        ]
    )
    q = (points - view.eye) @ view.basis.T
    distances = q @ view.planes[:, :3].T + view.planes[:, 3]
    if np.any(
        distances.max(axis=0)
        + view.movement_m * np.linalg.norm(view.planes[:, :3], axis=1)
        < 0
    ):
        return None
    nearest = float(q[:, 2].min()) - view.movement_m
    pixels = float("inf") if nearest <= 0 else view.focal_px * plan.width_m / nearest
    if pixels >= 8:
        return "through"
    if pixels < 0.5:
        return None
    from tile_facade_recess import recess_error

    directions = [
        (np.r_[tangent, 0.0], plan.guard_m),
        (np.r_[outward, 0.0], plan.guard_m),
        (np.array([0.0, 0.0, 1.0]), plan.slab_m / 2),
    ]
    error = max(
        recess_error(points, sign * axis, view, depth=size)
        for axis, size in directions
        for sign in (-1, 1)
    )
    return "plates" if error <= view.detail_error_px else "compact"
