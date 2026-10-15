"""Shared metric polygon primitives for room access and object placement."""

import math

import numpy as np
import shapely

AGENT_RADIUS_M = 0.30


def polygons(geometry):
    return [
        part
        for part in shapely.get_parts(geometry)
        if part.geom_type == "Polygon" and part.area > 1e-8
    ]


def edges(polygon, holes=True):
    polygon = shapely.orient_polygons(polygon)
    rings = [polygon.exterior, *polygon.interiors] if holes else [polygon.exterior]
    return [
        (np.asarray(a), np.asarray(b))
        for ring in rings
        for a, b in zip(ring.coords, list(ring.coords)[1:])
        if math.dist(a, b) > 1e-5
    ]
