"""Measure whole reference/render facets with independent cross-product normals."""

import math

import numpy as np
import shapely
from mvt_t_directional_facets import checked_crossfall


def requested_norm_limits(plan, model, crossfall_limit):
    if model not in ("whole_norm", "axis_crossfall"):
        raise ValueError("known declared profile gradient model required")
    if model == "whole_norm":
        return [row["gradient"] for row in plan["rows"]]
    stairs = {s[0] for s in plan["stairs"]}
    checked_crossfall(crossfall_limit)
    return [
        math.hypot(row["gradient"], crossfall_limit)
        if model == "axis_crossfall" and i not in stairs
        else row["gradient"]
        for i, row in enumerate(plan["rows"])
    ]


def _field(row, vertices, faces, crossfall_limit):
    triangles = vertices[faces]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if np.any(normal[:, 2] <= 0):
        raise ValueError(
            "directional audit found collapsed or reversed projected facets"
        )
    gradient = -normal[:, :2] / normal[:, 2:3]
    polygons = shapely.polygons(triangles[:, :, :2])
    covered = np.zeros(len(faces), bool)
    longitudinal, transverse = 0.0, 0.0
    for a, b in zip(row["xy"][:-1], row["xy"][1:], strict=True):
        line = shapely.LineString([a, b]).buffer(row["width_m"])
        hit = shapely.area(shapely.intersection(polygons, line)) > 1e-12
        covered |= hit
        tangent = (b - a) / np.linalg.norm(b - a)
        across = np.array([-tangent[1], tangent[0]])
        if hit.any():
            longitudinal = max(longitudinal, float(abs(gradient[hit] @ tangent).max()))
            transverse = max(transverse, float(abs(gradient[hit] @ across).max()))
    if not covered.all():
        raise ValueError("directional audit leaves a facet without a source axis")
    if longitudinal > row["gradient"] + 1e-7 or transverse > crossfall_limit + 1e-7:
        raise ValueError(
            "whole actual source face exceeds its longitudinal/crossfall limits"
        )
    return {
        "maximum_longitudinal_m_per_m": longitudinal,
        "maximum_crossfall_m_per_m": transverse,
    }


def audit_directional_profiles(plan, heights, data, crossfall_limit):
    checked_crossfall(crossfall_limit)
    stairs = {s[0] for s in plan["stairs"]}
    results = {}
    for owner, row in enumerate(plan["rows"]):
        if owner in stairs:
            continue
        top = data["top_faces"][(data["face_sources"] & (1 << owner)) != 0]
        results[row["source_id"]] = {
            "reference": _field(
                row, np.c_[plan["xy"], heights], plan["parts"][owner], crossfall_limit
            ),
            "render_float64": _field(row, data["body_vertices"], top, crossfall_limit),
            "render_float32": _field(
                row,
                data["body_vertices"].astype(np.float32).astype(float),
                top,
                crossfall_limit,
            ),
        }
    return results
