"""Inferred single-room halls on existing source facades, without a second grid."""

import numpy as np
import shapely
from outshine_ref.buildings.interiors.geometry import rectangle
from outshine_ref.buildings.roller_gates import RollerGate


def cargo_slot(row, entry, ceiling, clear):
    frame = entry["frame"]
    tangent = np.asarray(entry["door"]["tangent"])
    cell = entry["cell_width_m"]
    width, height = min(3.8, cell - 0.4), min(3.0, ceiling - 0.4)
    if width < 2.8 or height < 2.6:
        raise ValueError("Existing facade cell cannot hold the loading opening")
    columns = sorted(
        (c for c in range(frame["columns"]) if c != entry["column"]),
        key=lambda c: (abs(c - entry["column"]), c),
    )
    for column in columns:
        centre = np.asarray(frame["a"]) + (column + 0.5) * cell * tangent
        pocket = rectangle(centre, tangent, width, 0, 3.0)
        outside = rectangle(centre, tangent, width + 1, 4.0, 0)
        if (
            pocket.difference(row["polygon"]).area < 1e-8
            and outside.intersection(row["polygon"]).area < 1e-8
            and clear(row, outside, height)
        ):
            return {
                "id": row["id"] + ":floor-0:loading-0",
                "centre": centre.tolist(),
                "tangent": tangent.tolist(),
                "width_m": width,
                "height_m": height,
                "bottom_m": 0.0,
                "column": column,
                "facade_id": frame["id"],
                "pocket_wkb": pocket.wkb_hex,
                "provisional_outside_wkb": outside.wkb_hex,
            }
    raise ValueError(
        "No loading cell with an internal ramp and known-building clearance"
    )


def windows_for(row, facades, entry, gate, interior, ceiling, clear):
    result, omitted = [], []
    for frame in facades:
        cell = frame["width_m"] / frame["columns"]
        tangent = (np.asarray(frame["b"]) - frame["a"]) / frame["width_m"]
        width = cell * frame["fractions"][0]
        height = frame["height_m"] * frame["fractions"][1]
        bottom = (frame["height_m"] - height) / 2
        for column in range(frame["columns"]):
            identifier = frame["id"] + f":floor-0:window-{column}"
            special = frame["id"] == gate["facade_id"] and column in (
                entry["column"],
                gate["column"],
            )
            centre = np.asarray(frame["a"]) + (column + 0.5) * cell * tangent
            inside = rectangle(centre, tangent, width + 0.16, -0.24, 0.5)
            outside = rectangle(centre, tangent, width + 0.16, 0.2, 0)
            reason = (
                "replaced by personnel/loading opening"
                if special
                else "window lacks floor/ceiling frame margin"
                if bottom < 0.32 or bottom + height > ceiling - 0.10
                else "window lacks owned interior depth"
                if not interior.buffer(1e-8).covers(inside)
                else "window detail blocked by a known building"
                if outside.intersection(row["polygon"]).area > 1e-8
                or not clear(row, outside, bottom + height, low_m=bottom)
                else None
            )
            if reason:
                omitted.append({"id": identifier, "reason": reason})
                continue
            if len(result) >= 96:
                raise ValueError(
                    "Hall window capacity exceeded before geometry expansion"
                )
            result.append(
                {
                    "id": identifier,
                    "centre": centre.tolist(),
                    "tangent": tangent.tolist(),
                    "width_m": width,
                    "height_m": height,
                    "bottom_m": bottom,
                    "facade_id": frame["id"],
                    "column": column,
                }
            )
    return result, omitted


def plan(row, parameters, info, facades, entry, clear):
    if (
        "roof_section" not in info
        or row.get("clipped")
        or parameters["minHeight"] != 0
        or not 800 < row["polygon"].area <= 15000
        or not facades
        or any(f["rows"] != 1 for f in facades)
    ):
        return None
    if entry is None:
        raise ValueError(
            "A usable personnel entrance is required before loading access"
        )
    interior = row["polygon"].buffer(-0.24, join_style=2)
    walking = interior.buffer(-0.30, join_style=2)
    if (
        interior.geom_type != "Polygon"
        or walking.geom_type != "Polygon"
        or walking.is_empty
    ):
        raise ValueError("Hall interior has no connected human-clearance region")
    ceiling = info["eave_m"] - 0.18
    gate = cargo_slot(row, entry, ceiling, clear)
    definition = RollerGate(gate["width_m"], gate["height_m"])
    inward = np.array([-gate["tangent"][1], gate["tangent"][0]])
    arrival = np.asarray(entry["door"]["centre"]) + inward * 1.5
    cargo = np.asarray(gate["centre"]) + inward * 3.8
    corner = np.asarray(entry["door"]["centre"]) + inward * 3.8
    route = shapely.LineString([arrival, corner, cargo]).buffer(0.30)
    if not interior.buffer(1e-8).covers(route):
        raise ValueError("Personnel/loading path leaves the connected hall")
    windows, omitted = windows_for(row, facades, entry, gate, interior, ceiling, clear)
    return {
        "id": row["id"],
        "room_id": row["id"] + ":floor-0:room-0",
        "footprint_wkb": row["polygon"].wkb_hex,
        "interior_wkb": interior.wkb_hex,
        "floor_m": 0.16,
        "ceiling_m": ceiling,
        "wall_m": 0.24,
        "gate": gate,
        "definition": definition,
        "windows": windows,
        "omitted": omitted,
        "route_wkb": route.wkb_hex,
        "entry": entry,
        "origin": "Procedural single-room industrial interpretation on existing provider facade cells",
        "limits": "Outside clearance only against known buildings; no transport/terrain/vehicle-route admission",
    }


def metadata(value):
    return {k: v for k, v in value.items() if k not in ("entry", "definition")}
