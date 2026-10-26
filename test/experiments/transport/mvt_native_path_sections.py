"""Bounded interior path sections, clipped to their own prepared source domain."""

import numpy as np
import shapely


def sections(row, domain, spacing_m):
    if spacing_m is None:
        return []
    if not np.isfinite(spacing_m) or not 0.25 <= spacing_m <= 10:
        raise ValueError("bounded positive path section spacing required")
    if row["original"]["source"]["feature"]["properties"]["subclass"] == "steps":
        return []
    line = shapely.LineString(row["xy"])
    stations = np.arange(spacing_m, line.length - spacing_m / 4, spacing_m)
    if len(stations) > 256:
        raise ValueError("native path section work budget")
    result = []
    for station in stations:
        point = np.array(line.interpolate(station).coords[0])
        delta = min(1e-4, spacing_m / 16)
        a = np.array(line.interpolate(station - delta).coords[0])
        b = np.array(line.interpolate(station + delta).coords[0])
        tangent = (b - a) / np.linalg.norm(b - a)
        normal = np.array([-tangent[1], tangent[0]]) * 2 * row["width_m"]
        result.append(
            domain.intersection(shapely.LineString([point - normal, point + normal]))
        )
    return result
