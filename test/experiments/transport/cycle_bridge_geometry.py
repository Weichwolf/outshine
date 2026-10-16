"""Actual affine-ground ribbon with zero-height mouths and whole underside thickness."""

import numpy as np
import shapely
from cycle_bridge_arrangement import surface as arrangement_surface
from foot_ramp_geometry import Ramp, close_caps
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import ProjectedSurfaceBudget, projected_height_surface


def height_table(problem, heights):
    s, d = problem["stations"], problem["recipe"]["deck_thickness_m"]
    extra = []
    for a, b, x, y in zip(s[:-1], s[1:], heights[:-1], heights[1:], strict=True):
        if (x - d) * (y - d) < 0:
            extra.append(a + (b - a) * (d - x) / (y - x))
    stations = np.unique(np.r_[s, extra])
    return np.c_[stations, np.interp(stations, s, heights)]


def active_domain(problem, table):
    selected = (table[:-1, 1] > 1e-9) | (table[1:, 1] > 1e-9)
    indices = np.flatnonzero(selected)
    if not len(indices) or np.any(np.diff(indices) != 1):
        raise ValueError("one connected nonzero bridge ribbon required")
    lo, hi = table[indices[0], 0], table[indices[-1] + 1, 0]
    u, origin = problem["axis"], problem["origin"]
    v = np.array([-u[1], u[0]])
    extent = (shapely.get_coordinates(problem["domain"]) - origin) @ v
    points = np.array(
        [
            [lo, extent.min() - 1],
            [hi, extent.min() - 1],
            [hi, extent.max() + 1],
            [lo, extent.max() + 1],
        ]
    )
    region = shapely.Polygon(points @ np.array([u, v]) + origin)
    return problem["domain"].intersection(region)


def build(problem, heights, *, method="arrangement"):
    table = height_table(problem, heights)
    domain = active_domain(problem, table)
    if method == "arrangement":
        surface, note = arrangement_surface(
            domain, problem["origin"], problem["axis"], table
        )
    elif method == "slices":
        surface, note = projected_height_surface(
            domain,
            problem["origin"],
            problem["axis"],
            table,
            budget=ProjectedSurfaceBudget(time_limit_s=4, max_stations=512),
        )
    else:
        raise ValueError("known bridge geometry method required")
    if surface is None:
        raise ValueError(note["reason"])
    xy, faces = surface.vertices[:, :2], surface.triangles
    relative = surface.vertices[:, 2].copy()
    relative[abs(relative) <= 1e-8] = 0
    if np.any(relative < 0):
        raise ValueError("bridge ribbon dips below its original ground contact")
    reference = problem["reference"]
    ground = reference[2] + (xy - reference[:2]) @ problem["gradient"]
    bottom = np.maximum(relative - problem["recipe"]["deck_thickness_m"], 0)
    upper = Mesh(np.c_[xy, ground + relative], faces)
    lower = Mesh(np.c_[xy, ground + bottom], faces)
    return Ramp(close_caps(upper, lower), lower, upper, domain), dict(
        note, table=table.tolist()
    )
