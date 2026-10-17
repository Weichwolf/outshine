"""Source-contained attached box dormers, fitted to the actual sloped roof."""

from dataclasses import dataclass
import math

import numpy as np
import shapely
from dormer_roof import RoofSurface


@dataclass(frozen=True)
class Dormer:
    identifier: str
    footprint_wkb: str
    a: tuple
    b: tuple
    width_m: float
    base_m: float
    cap_m: float
    low_m: float
    added_volume_m3: float

    def frame(self):
        return {
            "id": self.identifier + "/front",
            "a": list(self.a),
            "b": list(self.b),
            "width_m": self.width_m,
            "height_m": self.cap_m - self.base_m,
            "base_m": self.base_m,
            "top_m": self.cap_m,
            "architecture_scope": "dormer",
            "columns": 1,
            "rows": 1,
            "fractions": [0.72, 0.64],
            "opening_count": 1,
        }


def plan(row, envelope, choice, *, clear=None, maximum=64):
    if type(maximum) is not int or not 1 <= maximum <= 64:
        raise ValueError("Explicit dormer capacity1..64 required")
    if (
        not np.isfinite([choice.eave_m, choice.rise_m]).all()
        or not row["polygon"].is_valid
        or row["polygon"].is_empty
    ):
        raise ValueError(
            "Dormers require finite roof parameters and a valid source polygon"
        )
    if choice.kind != "hip" or choice.rise_m < 2.8 or row.get("clipped"):
        return (), [{"reason": "outside resolved pitched dormer scope"}]
    source = shapely.orient_polygons(row["polygon"])
    roof = RoofSurface(envelope)
    cap = choice.eave_m + min(2.85, choice.rise_m - 0.3)
    width, depth, inset, pitch = 2.2, 1.6, 0.05, 6.0
    accepted, rejected, occupied = [], [], []
    for ring_index, ring in enumerate((source.exterior, *source.interiors)):
        xy = np.asarray(ring.coords)
        for edge, (start, end) in enumerate(zip(xy[:-1], xy[1:], strict=True)):
            length = float(np.linalg.norm(end - start))
            count = math.floor((length - 3) / pitch)
            if count < 1:
                continue
            tangent = (end - start) / length
            inward = np.array([-tangent[1], tangent[0]])
            for column in range(count):
                identifier = f"{row['id']}/dormer/{ring_index}/{edge}/{column}"
                centre = (
                    start + tangent * length * (column + 0.5) / count + inward * inset
                )
                a, b = centre - tangent * width / 2, centre + tangent * width / 2
                polygon = shapely.Polygon(
                    [a, b, b + inward * depth, a + inward * depth]
                )
                try:
                    if not source.covers(polygon):
                        raise ValueError("dormer reaches a source edge or courtyard")
                    if any(polygon.distance(p) < 0.5 for p in occupied):
                        raise ValueError("dormers need a half-metre clear separation")
                    values = [roof.height(p) for p in (a, b, centre)]
                    if np.ptp(values) > 1e-7 or cap - max(values) < 2.0:
                        raise ValueError(
                            "actual eave face leaves no rectangular useful front"
                        )
                    volume = roof.added_volume(polygon, cap)
                    if volume < 0.8:
                        raise ValueError("dormer adds no meaningful roof volume")
                    if clear is not None and not clear(
                        polygon, choice.eave_m - 0.04, cap
                    ):
                        raise ValueError("new roof volume overlaps an existing source")
                    if len(accepted) >= maximum:
                        raise ValueError("whole-source dormer capacity exceeded")
                    accepted.append(
                        Dormer(
                            identifier,
                            polygon.wkb_hex,
                            tuple(a),
                            tuple(b),
                            width,
                            values[0],
                            cap,
                            choice.eave_m - 0.04,
                            volume,
                        )
                    )
                    occupied.append(polygon)
                except ValueError as error:
                    rejected.append({"id": identifier, "reason": str(error)})
    return tuple(accepted), rejected
