"""Source boundary facade cells before surface generation; the mesh binder shares them."""

import math

import numpy as np
import shapely
from outshine_ref.buildings.facades import FacadeRepeat


def layout(row, parameters, fractions, axis_m, floor_m):
    top = float(parameters.get("eave_m", parameters["buildingHeight"]))
    height = top - parameters["minHeight"]
    rows = max(1, math.floor(height / floor_m + 0.5))
    result = []
    polygon = shapely.orient_polygons(row["polygon"])
    for ring in (polygon.exterior, *polygon.interiors):
        for a, b in zip(list(ring.coords)[:-1], list(ring.coords)[1:], strict=True):
            a, b = np.asarray(a), np.asarray(b)
            length = float(np.linalg.norm(b - a))
            if length == 0:
                continue
            columns = max(1, math.floor(length / axis_m + 0.5))
            repeat = FacadeRepeat(length, height, columns, rows, *fractions)
            result.append(
                {
                    "id": row["id"] + f"/facade-{len(result)}",
                    "a": a.tolist(),
                    "b": b.tolist(),
                    "width_m": length,
                    "height_m": height,
                    "columns": columns,
                    "rows": rows,
                    "fractions": list(fractions),
                    "opening_count": repeat.opening_count,
                }
            )
    return result
