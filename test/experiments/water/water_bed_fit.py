"""Reuse the physical water fitter for bounded bed facets below their water."""

from dataclasses import replace

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.places.sources import tile_geography
from outshine_ref.water import fit_physical_surface
from outshine_ref.water._fit import grade_envelopes
from outshine_ref.water.frame import normal_caps, tangent_directions
from physical_boundary import from_world, to_world
from water_bed_mesh import bed_from_vertices


def bed_boxes(layout, local, water_local, max_depth_m):
    original = water_local[layout.water.faces]
    normal = np.cross(original[:, 1] - original[:, 0], original[:, 2] - original[:, 0])
    if np.any(normal[:, 2] <= 0):
        raise ValueError("upward water facets in canonical cell frame required")
    parent = layout.parent_faces
    corners = local[layout.faces, :2] - original[parent, :1, :2]
    upper_at = (
        original[parent, 0, 2, None]
        - np.einsum("fij,fj->fi", corners, normal[parent, :2]) / normal[parent, 2, None]
    )
    upper = np.full(len(local), np.inf)
    np.minimum.at(upper, layout.faces.ravel(), upper_at.ravel())
    lower = upper - max_depth_m
    fixed = np.unique(layout.boundary)
    allowance = 64 * np.spacing(max(1.0, float(abs(layout.top_vertices).max())))
    if np.any(local[fixed, 2] > upper[fixed] + allowance) or np.any(
        local[fixed, 2] < lower[fixed] - allowance
    ):
        raise ValueError("fixed bed port outside declared depth envelope")
    upper[fixed] = np.maximum(upper[fixed], local[fixed, 2])
    lower[fixed] = np.minimum(lower[fixed], local[fixed, 2])
    return np.c_[lower, upper], fixed


def directional_rows(faces, grade):
    rows = np.zeros((2 * faces, 6))
    rows[:, 0] = np.repeat(np.arange(faces), 2)
    rows[::2, 1] = 1
    rows[1::2, 2] = 1
    rows[:, 4] = -grade
    rows[:, 5] = grade
    return rows


def fitted_bed(
    layout, initial, *, directional_grade, max_depth_m, seconds, grade_reserve
):
    if (
        not np.isfinite(directional_grade)
        or not 0 < directional_grade <= 2
        or not np.isfinite(max_depth_m)
        or not 0 < max_depth_m <= 100
        or not np.isfinite(grade_reserve)
        or not 0 <= grade_reserve < 0.5
    ):
        raise ValueError("explicit physical bed grade and depth bounds required")
    lat, lon = tile_geography([[0.5, 0.5]], 1, layout.water.cell)
    anchor = [lon[0], lat[0]]
    local = from_world(anchor, initial.vertices)
    boxes, fixed = bed_boxes(
        layout, local, from_world(anchor, layout.water.world), max_depth_m
    )
    rows = directional_rows(len(layout.faces), directional_grade * (1 - grade_reserve))
    prism = (float(boxes[:, 0].min()), float(boxes[:, 1].max()))
    fitted = fit_physical_surface(
        Mesh(local, layout.faces),
        anchor,
        rows,
        up_bounds_m=prism,
        fixed={int(i): float(local[i, 2]) for i in fixed},
        vertex_up_bounds_m=boxes,
        seconds=seconds,
    )
    if fitted.mesh is None:
        raise ValueError("bed fit refused: " + str(fitted.report["reason"]))
    world = to_world(anchor, fitted.mesh.vertices)
    world[fixed] = initial.vertices[fixed]
    result = bed_from_vertices(
        layout, world, target_depth_m=initial.report["maximum_target_depth_m"]
    )
    actual = from_world(anchor, world)
    box_error = max(
        float((boxes[:, 0] - actual[:, 2]).max(initial=0)),
        float((actual[:, 2] - boxes[:, 1]).max(initial=0)),
        0.0,
    )
    if box_error > 1e-9:
        raise ValueError("materialized bed violates vertex height bounds")
    tri = actual[layout.faces]
    normal = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    normal /= normal[:, 2, None]
    actual_prism = (
        min(prism[0], float(actual[:, 2].min())),
        max(prism[1], float(actual[:, 2].max())),
    )
    caps = normal_caps(anchor, actual[:, :2], layout.faces, actual_prism)
    owners = rows[:, 0].astype(int)
    tangent = tangent_directions(caps["up"][owners], rows[:, 1:4])
    envelope = grade_envelopes(normal, caps, owners, tangent)
    excess = max(
        float(envelope.max() - directional_grade),
        float(-directional_grade - envelope.min()),
        0.0,
    )
    if excess > 1e-9:
        raise ValueError(
            f"materialized bed violates physical directional grade by {excess:.9g}"
        )
    return replace(
        result,
        report=dict(
            result.report,
            physical_fit=dict(fitted.report),
            materialized_grade_excess=excess,
            materialized_vertex_bound_excess_m=box_error,
            directional_grade_limit=directional_grade,
            solve_grade_limit=directional_grade * (1 - grade_reserve),
            grade_reserve_fraction=grade_reserve,
            maximum_vertex_up_depth_m=max_depth_m,
            fixed_boundary_vertices=len(fixed),
        ),
    )
