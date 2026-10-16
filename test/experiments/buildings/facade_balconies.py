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
        "height_interval_m": (base, base + 0.16 + prototype.guard_height_m),
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

    @staticmethod
    def demand(current, view):
        return demand(current, view)

    def products(self, current, material, mode):
        from balcony_geometry import products

        return products(current, self, material, mode)

    @staticmethod
    def description(current, mode, triangles, frame):
        return {
            "id": current["id"],
            "kind": "balcony",
            "facade_id": frame["id"],
            "cell": [current["column"], current["row"]],
            "door_id": current["door_id"],
            "mode": mode,
            "cells": int(mode == "through"),
            "angle_deg": current["doorway"]["angle_deg"],
            "triangles": triangles,
            "scope": "Same local deck/guard and upper door; full storey route and native interaction unproved",
        }

    def plan(self, row, entry, family):
        from upper_facade_cells import propose

        planned = propose(
            row,
            entry,
            family,
            cell,
            self.states,
            self.entrances,
            self.rejected,
            "Balcony",
        )
        frame = entry["frame"] if entry else None
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
