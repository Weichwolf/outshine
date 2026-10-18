"""Source-repeat hierarchy; no new window grid or camera-dependent architecture."""

from dataclasses import dataclass

import numpy as np
from outshine_ref.buildings.facade_frames import FacadeFrame


@dataclass(frozen=True)
class Placement:
    facade_id: str
    kind: str
    frame: FacadeFrame
    origin_xy: tuple
    tangent: tuple
    base_m: float


def placement(facade, bottom, family):
    width, height = facade["width_m"], facade["height_m"]
    columns, rows = facade["columns"], facade["rows"]
    if not 4 <= columns <= 24 or not 3 <= rows <= 16:
        raise ValueError("Frame programme requires 4..24 columns and 3..16 storeys")
    cell, floor = width / columns, height / rows
    gap_u, gap_v = np.array([cell, floor]) * (1 - np.asarray(facade["fractions"]))
    margin, pier, beam = 0.12, 0.24, 0.22
    if margin + pier > gap_u / 2 - 0.10 or beam + 0.15 > gap_v / 2 - 0.10:
        raise ValueError("Frame members leave insufficient existing opening clearance")
    start = floor - beam / 2
    extent = height - 0.15 - start
    run = width - 2 * margin
    vertical = (
        pier / 2,
        *(c * cell - margin for c in range(2, columns, 2)),
        run - pier / 2,
    )
    kind = "paired-piers" if family == "residential" else "storey-grid"
    intermediate = (
        ()
        if kind == "paired-piers"
        else tuple(floor * r - start for r in range(2, rows))
    )
    horizontal = (beam / 2, *intermediate, extent - beam / 2)
    plan = FacadeFrame(run, extent, vertical, horizontal, pier, beam)
    tangent = (np.asarray(facade["b"]) - facade["a"]) / width
    origin = np.asarray(facade["a"]) + margin * tangent
    return Placement(
        facade["id"], kind, plan, tuple(origin), tuple(tangent), bottom + start
    )


def choose(row, parameters, facades, family, entry, *, max_triangles=4096):
    if type(max_triangles) is not int or max_triangles < 0:
        raise ValueError("Frame programme capacity requires a nonnegative integer")
    if (
        family not in ("residential", "mixed-use", "commercial", "institutional")
        or row.get("clipped")
        or parameters["minHeight"] != 0
        or entry is None
    ):
        return (), []
    candidates = sorted(
        (f for f in facades if f["id"] != entry["frame"]["id"]),
        key=lambda f: (-f["width_m"], f["id"]),
    )
    plans, rejected = [], []
    remaining = max_triangles
    for facade in candidates[:4]:
        try:
            current = placement(facade, parameters["minHeight"], family)
            count = current.frame.triangle_count(True)
            if count > remaining:
                raise ValueError("Whole-source logical frame capacity exceeded")
            remaining -= count
            plans.append(current)
        except ValueError as error:
            rejected.append(
                {"id": row["id"], "facade_id": facade["id"], "reason": str(error)}
            )
    return tuple(plans), rejected
