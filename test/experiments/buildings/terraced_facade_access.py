"""Continuous complete ground arrival, including new closed facade components."""

from itertools import pairwise
import math

import numpy as np
import shapely

from building_plan import FLOOR_M
from entrance_clearance import clearance, collision, section


def arrival_parts(mass, asset):
    centre, tangent = np.asarray(mass.entry.centre), np.asarray(mass.entry.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    path = shapely.LineString([centre + 0.8 * normal, centre - 1.5 * normal])
    radius = 0.30 / math.cos(math.pi / (4 * 32))
    region = path.buffer(radius, quad_segs=32)
    mesh = asset.frames
    triangles = mesh["vertices"][mesh["faces"]]
    lo, hi = triangles[:, :, :2].min(axis=1), triangles[:, :, :2].max(axis=1)
    x0, y0, x1, y1 = region.bounds
    selected = (hi[:, 0] >= x0) & (hi[:, 1] >= y0) & (lo[:, 0] <= x1) & (lo[:, 1] <= y1)
    selected &= triangles[:, :, 2].min(axis=1) <= mass.bottom_m + FLOOR_M + 1.90
    selected &= triangles[:, :, 2].max(axis=1) >= mass.bottom_m + FLOOR_M
    parts = [asset.floors.shell.mesh, asset.floors.landing.mesh]
    for part in np.unique(mesh["parts"][selected]):
        rows = np.flatnonzero(mesh["parts"] == part)
        hull = shapely.MultiPoint(triangles[rows, :, :2].reshape(-1, 2)).convex_hull
        if hull.intersects(region):
            parts.append({"vertices": mesh["vertices"], "faces": mesh["faces"][rows]})
    return path, parts


def check_arrival(mass, asset):
    path, parts = arrival_parts(mass, asset)
    floor = mass.bottom_m + FLOOR_M
    proof = clearance(
        {"door": {"centre": mass.entry.centre, "tangent": mass.entry.tangent}},
        parts,
        floor_z_m=floor,
    )
    solid = collision(parts)
    heights = np.unique(np.asarray(solid.to_mesh64().vert_properties)[:, 2])
    scaled = solid.scale((1000.0,) * 3)
    radius = 0.10 / math.cos(math.pi / (4 * 32))
    legs = path.buffer(radius, quad_segs=32)
    for lo, hi in pairwise(heights):
        low, high = max(float(lo), floor + 1e-7), min(float(hi), floor + 0.40)
        if (
            low < high
            and legs.intersection(section(scaled, (low + high) / 2)).area > 1e-8
        ):
            raise ValueError(
                "Actual facade obstructs the continuous lower-leg/foot tube"
            )
    return proof | {
        "additional_closed_facade_parts": len(parts) - 2,
        "continuous_lower_legs_checked": True,
    }
