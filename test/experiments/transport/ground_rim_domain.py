"""Explicit outer rectangles; minimum area is claimed only before margin."""

import numpy as np
import shapely


def rim_rectangle(xy, margin, boundary):
    if boundary == "axis":
        return shapely.box(*(xy.min(axis=0) - margin), *(xy.max(axis=0) + margin))
    if boundary != "oriented":
        raise ValueError("Known rim rectangle boundary required")
    points = shapely.MultiPoint(xy)
    rectangle = shapely.normalize(shapely.oriented_envelope(points))
    if rectangle.geom_type != "Polygon" or rectangle.is_empty:
        raise ValueError("Nondegenerate oriented rim rectangle required")
    corners = np.asarray(rectangle.exterior.coords)[:-1]
    sides = np.roll(corners, -1, axis=0) - corners
    axis = sides[np.argmax(np.linalg.norm(sides, axis=1))]
    axis = axis / np.linalg.norm(axis)
    if axis[0] < 0 or (axis[0] == 0 and axis[1] < 0):
        axis = -axis
    axes = np.column_stack((axis, [-axis[1], axis[0]]))
    local = xy @ axes
    low, high = local.min(axis=0) - margin, local.max(axis=0) + margin
    corners = np.array([low, [high[0], low[1]], high, [low[0], high[1]]])
    domain = shapely.Polygon(corners @ axes.T)
    if not domain.is_valid or not domain.contains(points):
        raise ValueError("Oriented rim rectangle does not strictly contain road")
    return domain
