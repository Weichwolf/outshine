"""Declared longitudinal/crossfall alternative; whole-norm remains the default."""

import math
from itertools import pairwise

import numpy as np
import shapely
from stair_junction_solve import facets


def checked_crossfall(value):
    if not math.isfinite(value) or not 0.002 < value <= 0.2:
        raise ValueError("bounded explicit procedural crossfall limit required")


def _gradients(xy, faces):
    points = xy[faces]
    return np.linalg.inv(points[:, 1:] - points[:, :1]) @ np.array(
        [[-1, 1, 0], [-1, 0, 1]]
    )


def _append_directional_rows(
    system, faces, coefficients, limits, epsilon, cast_coefficients
):
    bounds = np.broadcast_to(
        limits - np.where(limits > 0.1, 0.002, 0.0002), coefficients.shape[:2]
    )
    if cast_coefficients is not None:
        cast_bounds = limits - epsilon * abs(cast_coefficients).sum(axis=2)
        if (cast_bounds < 0).any():
            raise ValueError(
                "directional path face requires a better conditioned precision mesh"
            )
        coefficients = np.concatenate((coefficients, cast_coefficients), axis=1)
        bounds = np.concatenate((bounds, cast_bounds), axis=1)
    rows = coefficients.shape[1]
    columns = np.broadcast_to(faces[:, None, :], (len(faces), rows, 3))
    system.add_rows(columns.reshape(-1, 3), coefficients.reshape(-1, 3), bounds.ravel())
    return len(faces) * rows


def directional_facets(
    plan,
    system,
    directions,
    reserve,
    flat,
    precision,
    *,
    height_bound_m,
    crossfall_limit,
):
    checked_crossfall(crossfall_limit)
    stairs = {s[0] for s in plan["stairs"]}
    stair_plan = dict(
        plan,
        parts=[
            f if i in stairs else np.empty((0, 3), int)
            for i, f in enumerate(plan["parts"])
        ],
    )
    facets(
        stair_plan,
        system,
        directions,
        reserve,
        flat,
        precision,
        height_bound_m=height_bound_m,
    )
    cast = plan["xy"].astype(np.float32).astype(float)
    epsilon = float(np.spacing(np.float32(height_bound_m))) / 2
    work = {"segment_face_candidates": 0, "directional_faces": 0, "directional_rows": 0}
    for owner, (row, faces) in enumerate(zip(plan["rows"], plan["parts"], strict=True)):
        if owner in stairs:
            continue
        shapes = shapely.polygons(plan["xy"][faces])
        covered = np.zeros(len(faces), bool)
        gradients = _gradients(plan["xy"], faces)
        cast_gradients = _gradients(cast, faces) if precision else None
        for a, b in pairwise(row["xy"]):
            tangent = (b - a) / np.linalg.norm(b - a)
            normal = np.array([-tangent[1], tangent[0]])
            vectors = np.array([tangent, -tangent, normal, -normal])
            influence = shapely.LineString([a, b]).buffer(row["width_m"])
            work["segment_face_candidates"] += len(faces)
            hit = np.flatnonzero(
                shapely.area(shapely.intersection(shapes, influence)) > 1e-12
            )
            covered[hit] = True
            limits = np.array(
                [row["gradient"], row["gradient"], crossfall_limit, crossfall_limit]
            )
            coefficients = vectors @ gradients[hit]
            cast_coefficients = vectors @ cast_gradients[hit] if precision else None
            work["directional_faces"] += len(hit)
            work["directional_rows"] += _append_directional_rows(
                system, faces[hit], coefficients, limits, epsilon, cast_coefficients
            )
        if not covered.all():
            raise ValueError(
                "directional grade leaves a source face without an axis segment"
            )
    return work
