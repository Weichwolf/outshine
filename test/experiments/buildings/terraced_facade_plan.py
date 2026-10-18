"""Source-stable metric window axes on every actual stepped profile edge."""

from dataclasses import dataclass
from itertools import pairwise
import math

import numpy as np
import shapely

from building_layout import WALL_M, rectangle
from building_plan import CEILING_M, FLOOR_M


@dataclass(frozen=True)
class Window:
    identifier: str
    storey: int
    edge: tuple
    station: int
    centre: tuple
    tangent: tuple
    width_m: float
    bottom_m: float
    height_m: float
    cutter_wkb: bytes


@dataclass(frozen=True)
class Facade:
    identifier: str
    programme: str
    pitch_m: float
    windows: tuple
    rejected: tuple


def candidates(mass):
    anchor = np.asarray(shapely.from_wkb(mass.source_wkb).centroid.coords[0])
    pitch, width, sill, height = (
        (3.4, 2.25, 0.78, 1.50) if mass.kind == "cascade" else (2.8, 1.45, 0.62, 1.78)
    )
    core = shapely.from_wkb(mass.core.footprint_wkb).buffer(0.12)
    entrance = shapely.from_wkb(mass.entry.opening_wkb).buffer(0.12)
    accepted, rejected = [], []
    for index, storey in enumerate(mass.storeys):
        polygon = shapely.orient_polygons(shapely.from_wkb(storey.footprint_wkb))
        low = storey.bottom_m + FLOOR_M + sill
        high = min(low + height, storey.top_m - CEILING_M - 0.20)
        for ring_index, ring in enumerate((polygon.exterior, *polygon.interiors)):
            for edge_index, (a, b) in enumerate(pairwise(np.asarray(ring.coords))):
                delta = b - a
                length = float(np.linalg.norm(delta))
                if length < width + 0.8:
                    continue
                tangent = delta / length
                phase = float((a - anchor) @ tangent)
                first = math.ceil((phase + 0.4 + width / 2) / pitch)
                last = math.floor((phase + length - 0.4 - width / 2) / pitch)
                for station in range(first, last + 1):
                    centre = a + (station * pitch - phase) * tangent
                    pocket = rectangle(centre, tangent, width + 0.12, 0, WALL_M + 0.42)
                    outside = rectangle(centre, tangent, width + 0.12, 0.08, 0)
                    cutter = rectangle(centre, tangent, width, 0.06, WALL_M + 0.04)
                    identifier = (
                        storey.identifier
                        + f"/window/{ring_index}/{edge_index}/{station}"
                    )
                    reason = None
                    if high - low < 0.8 or not polygon.buffer(1e-8).covers(pocket):
                        reason = "insufficient straight wall/interior margin"
                    elif outside.intersection(polygon).area > 1e-8:
                        reason = "opposing source wall in outward margin"
                    elif cutter.intersection(core).area > 1e-8:
                        reason = "reserved stair core"
                    elif index == 0 and cutter.intersection(entrance).area > 1e-8:
                        reason = "existing entrance"
                    if reason:
                        rejected.append((identifier, reason))
                        continue
                    if len(accepted) >= 768:
                        raise ValueError("Terraced facade exceeds768 actual openings")
                    accepted.append(
                        Window(
                            identifier,
                            index,
                            (ring_index, edge_index),
                            station,
                            tuple(centre),
                            tuple(tangent),
                            width,
                            low,
                            high - low,
                            cutter.wkb,
                        )
                    )
    return Facade(
        mass.identifier + "/facade", mass.kind, pitch, tuple(accepted), tuple(rejected)
    )
