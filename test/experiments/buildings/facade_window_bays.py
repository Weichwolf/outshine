"""Window bays on the shared upper-cell placement, with full-height context checks."""

import numpy as np
import shapely
from outshine_ref.buildings.interiors.geometry import rectangle
from outshine_ref.buildings.window_bays import (
    WindowBay,
    bay_skin,
    bay_surface,
    sections,
)
from shapely.affinity import affine_transform
from upper_facade_cells import propose


def cell(entry, column, level, fraction):
    frame = entry["frame"]
    tangent = np.asarray(entry["door"]["tangent"])
    centre = np.asarray(frame["a"]) + (column + 0.5) * entry["cell_width_m"] * tangent
    prototype = WindowBay(
        min(3.2, 0.9 * entry["cell_width_m"]),
        entry["floor_m"],
        depth_m=0.9,
        front_fraction=fraction,
        head_m=min(2.25, entry["floor_m"] - 0.35),
    )
    shape = sections(prototype)
    transform = [tangent[0], -tangent[1], tangent[1], tangent[0], *centre]
    outside = shape["outer"].intersection(
        shapely.box(-prototype.width_m, -prototype.depth_m, prototype.width_m, 0)
    )
    width = prototype.width_m - 2 * prototype.wall_m
    base = level * entry["floor_m"]
    return {
        "id": entry["id"].removesuffix(":floor-0:entrance")
        + f":floor-{level}:bay-{column}",
        "column": column,
        "row": level,
        "base_m": base,
        "floor_z_m": base + prototype.floor_m,
        "height_interval_m": (base, base + prototype.height_m),
        "prototype": prototype,
        "frame": frame,
        "centre": centre,
        "tangent": tangent,
        "footprint": affine_transform(outside, transform),
        "full_footprint": affine_transform(shape["outer"], transform),
        "inside": affine_transform(shape["inside"], transform),
        "pocket": rectangle(centre, tangent, width, 0, 1.84),
        "clear_pocket": rectangle(centre, tangent, width, -0.25, 1.84),
        "cell_width_m": entry["cell_width_m"],
    }


class WindowBayDetails:
    def __init__(self, entrances):
        self.entrances = entrances
        self.planned, self.rejected, self.templates = [], [], {}

    def template(self, plan, mode):
        key = plan, mode
        if key not in self.templates:
            self.templates[key] = (
                bay_skin(plan)
                if mode == "compact"
                else bay_surface(plan, method="parts")
            )
        return self.templates[key]

    def plan(self, row, entry, family, fraction):
        if entry is not None and entry["frame"]["rows"] > 12:
            return None

        def factory(entry, column, level, _states):
            return cell(entry, column, level, fraction)

        planned = propose(
            row,
            entry,
            family,
            factory,
            {},
            self.entrances,
            self.rejected,
            "Window bay",
            max_levels=11,
            max_stacks=1,
        )
        if not planned:
            return None
        frame = entry["frame"]
        self.planned.append(
            {
                "source_id": row["id"],
                "facade_id": frame["id"],
                "origin": "Procedural residential bay programme, not supplied source geometry",
                "front_fraction": fraction,
                "cells": [
                    {
                        "id": c["id"],
                        "cell": [c["column"], c["row"]],
                        "floor_z_m": c["floor_z_m"],
                        "width_m": c["prototype"].width_m,
                        "depth_m": c["prototype"].depth_m,
                        "outside_footprint_wkb": c["footprint"].wkb_hex,
                        "height_interval_m": list(c["height_interval_m"]),
                    }
                    for c in planned
                ],
            }
        )
        return {"frame": frame, "entry": entry, "cells": planned, "owner": self}

    @staticmethod
    def demand(current, view):
        return demand(current, view)

    def products(self, current, material, mode):
        from window_bay_geometry import products

        return products(current, self, material, mode)

    @staticmethod
    def description(current, mode, triangles, frame):
        return {
            "id": current["id"],
            "kind": "window_bay",
            "facade_id": frame["id"],
            "cell": [current["column"], current["row"]],
            "mode": mode,
            "cells": int(mode == "through"),
            "triangles": triangles,
            "front_fraction": current["prototype"].front_fraction,
            "scope": "Actual open-back bay and local supported pocket; no full upper route or native/LOD/light admission",
        }


def demand(current, view):
    low, high = current["height_interval_m"]
    if low <= view.eye[2] <= high and (
        current["full_footprint"].covers(shapely.Point(view.eye[:2]))
        or current["pocket"].covers(shapely.Point(view.eye[:2]))
    ):
        return "through"
    bounds = shapely.union_all([current["full_footprint"], current["pocket"]]).bounds
    points = np.array(
        [
            [x, y, z]
            for x in (bounds[0], bounds[2])
            for y in (bounds[1], bounds[3])
            for z in (low, high)
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
    pixels = (
        float("inf")
        if nearest <= 0
        else view.focal_px * current["prototype"].width_m / nearest
    )
    return "through" if pixels >= 8 else "compact" if pixels >= 0.5 else None
