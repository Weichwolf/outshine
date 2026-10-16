"""Whole profile/ground arrangement with one indexed owner at each shared seam."""

import numpy as np
from cycle_bridge_arrangement import surface
from cycle_bridge_geometry import active_domain, height_table
from foot_ramp_geometry import Ramp, close_caps
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


def reduced_heights(stations, heights):
    keep = np.ones(len(stations), bool)
    for i in range(1, len(stations) - 1):
        expected = heights[i - 1] + (heights[i + 1] - heights[i - 1]) * (
            stations[i] - stations[i - 1]
        ) / (stations[i + 1] - stations[i - 1])
        if abs(expected - heights[i]) <= 1e-12:
            keep[i] = False
    if (
        np.max(abs(np.interp(stations, stations[keep], heights[keep]) - heights))
        > 1e-10
    ):
        raise ValueError("profile knot reduction changes the verified height field")
    return stations[keep], heights[keep]


def build(problem, heights, field):
    s, h = reduced_heights(problem["stations"], heights)
    p = dict(problem, stations=s)
    table = height_table(p, h)
    domain = active_domain(p, table)
    template, note = surface(domain, p["origin"], p["axis"], table, field=field)
    xy, faces = template.vertices[:, :2], template.triangles
    contact = SurfaceSampler(field).sample(xy)
    if any(contact.reasons):
        raise ValueError("new ribbon leaves its actual shared ground field")
    relative = template.vertices[:, 2].copy()
    relative[abs(relative) <= 1e-8] = 0
    if np.any(relative < 0):
        raise ValueError("negative relative height in an above-ground candidate")
    ground = contact.points_enu_m[:, 2]
    lower = Mesh(
        np.c_[xy, ground + np.maximum(relative - p["recipe"]["deck_thickness_m"], 0)],
        faces,
    )
    upper = Mesh(np.c_[xy, ground + relative], faces)
    return Ramp(close_caps(upper, lower), lower, upper, domain), dict(
        note, table=table.tolist()
    )
