"""Minimum 2D rectangles, reusing axis-aligned bounds whose minimum is proven."""

import numpy as np
import shapely


def minimum_rectangle(geometry, coordinates=None):
    """Keep the original GEOS call unless every axis-box corner is present.

    All input coordinates lie in their bounds. If all four corners are also
    input coordinates, their convex hull is exactly that box. No hull search,
    area search, tolerance or change to other polygons' GEOS input is needed.
    Starting-corner normalization remains the caller's existing responsibility.
    Optional coordinates borrow the exact XY array already used to construct or
    validate this geometry; they must describe this same geometry.
    """
    points = shapely.get_coordinates(geometry) if coordinates is None else coordinates
    if len(points):
        low, high = points.min(axis=0), points.max(axis=0)
        corners = (
            (low[0], low[1]),
            (low[0], high[1]),
            (high[0], low[1]),
            (high[0], high[1]),
        )
        if all(np.any(np.all(points == corner, axis=1)) for corner in corners):
            if np.any(low == high):
                return geometry.convex_hull
            return geometry.envelope
    return shapely.minimum_rotated_rectangle(geometry)
