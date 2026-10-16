"""A bounded real-DEM partition around unchanged selected ground undersides."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, conform_triangles
from outshine_ref.transport import HeightBudget, fit_surface_heights
from stair_network_inputs import sample_xy
from surface_height_checks import normal_gradients


def selected_surfaces(data, rows):
    bits = sum(1 << i for i, row in enumerate(rows) if not row["original"]["bridge"])
    selected = (data["face_sources"] & bits) != 0
    vertices = data["body_vertices"]
    return (
        Mesh(vertices, data["bottom_faces"][selected]),
        Mesh(vertices, data["bottom_faces"][~selected]),
    )


def partition(ground, context, raster, *, spacing_m=12.0, margin_m=15.0):
    if not 4 <= spacing_m <= 24 or not 8 <= margin_m <= 24:
        raise ValueError("bounded research grid and explicit domain margin required")
    source = ground.vertices[ground.triangles, :2]
    polygons = shapely.polygons(source)
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("selected ground undersides are not one XY coverage")
    footprint = shapely.coverage_union_all(polygons)
    low = np.array(footprint.bounds[:2]) - margin_m
    high = np.array(footprint.bounds[2:]) + margin_m
    axes = [
        np.linspace(a, b, int(np.ceil((b - a) / spacing_m)) + 1)
        for a, b in zip(low, high, strict=True)
    ]
    xs, ys = axes
    ix, iy = np.meshgrid(np.arange(len(xs) - 1), np.arange(len(ys) - 1))
    ix, iy = ix.ravel(), iy.ravel()
    if len(ix) > 16384:
        raise ValueError("soil input cell cap exhausted")
    cells = shapely.box(xs[ix], ys[iy], xs[ix + 1], ys[iy + 1])
    triangles = shapely.get_parts(
        shapely.constrained_delaunay_triangles(shapely.difference(cells, footprint))
    )
    points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    if len(points) + len(source) > 32768:
        raise ValueError("soil input face cap exhausted")
    xy, indices = np.unique(
        np.vstack([source.reshape(-1, 2), points.reshape(-1, 2)]),
        axis=0,
        return_inverse=True,
    )
    if len(xy) > 16384:
        raise ValueError("soil vertex cap exhausted")
    faces, kinds, notes = [], [], []
    for kind, original in (
        (True, indices[: source.size // 2].reshape(-1, 3)),
        (False, indices[source.size // 2 :].reshape(-1, 3)),
    ):
        refined, note = conform_triangles(
            xy, original, max_candidates=2000000, max_faces=32768
        )
        p = xy[refined]
        u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        signed = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
        refined[signed < 0] = refined[signed < 0][:, ::-1]
        faces.extend(refined)
        kinds.extend([kind] * len(refined))
        notes.append(note)
    faces, kinds = np.array(faces), np.array(kinds)
    if len(faces) > 32768 or not shapely.coverage_is_valid(shapely.polygons(xy[faces])):
        raise ValueError("soil partition cap or edge-matched coverage failed")
    targets = sample_xy(xy, context, raster)
    samples = SurfaceSampler(ground).sample(
        xy, max_queries=16384, max_candidates=2000000
    )
    bed = samples.points_enu_m[:, 2]
    if not np.isfinite(bed[faces[kinds]]).all():
        raise ValueError("actual whole ground contact sample is unresolved")
    outer = np.any(
        np.isclose(xy, low, rtol=0, atol=1e-9)
        | np.isclose(xy, high, rtol=0, atol=1e-9),
        axis=1,
    )
    return {
        "xy": xy,
        "faces": faces,
        "ground": kinds,
        "targets": targets,
        "bed": bed,
        "outer": outer,
    }, {
        "spacing_m": spacing_m,
        "margin_m": margin_m,
        "cells": len(cells),
        "vertices": len(xy),
        "faces": len(faces),
        "domain_bounds_m": [*low, *high],
        "domain_area_m2": float(np.prod(high - low)),
        "interface_conformance": notes,
    }


def gentle_candidate(layout):
    xy, faces, targets, bed = (layout[k] for k in ("xy", "faces", "targets", "bed"))
    fixed = np.isfinite(bed)
    pins = {i: float(bed[i]) for i in np.flatnonzero(fixed)}
    pins.update({i: float(targets[i]) for i in np.flatnonzero(layout["outer"])})
    prescribed = targets.copy()
    for i, z in pins.items():
        prescribed[i] = z
    fully_fixed = np.isin(faces, list(pins)).all(1)
    selected = faces[fully_fixed]
    grades = np.linalg.norm(normal_gradients(xy, selected, prescribed), axis=1)
    witness = int(np.argmax(grades))
    result = fit_surface_heights(
        Mesh(np.c_[xy, targets], faces),
        np.tile(np.eye(2) / (2 / 3), (len(faces), 1, 1)),
        fixed=pins,
        budget=HeightBudget(time_limit_s=8, max_vertices=16384, max_faces=32768),
    )
    return {
        "fit": dict(result.report),
        "has_checked_surface": result.mesh is not None,
        "soil_grade_recipe": 2 / 3,
        "fully_fixed_face_grade": float(grades[witness]),
        "fully_fixed_face_xyz_m": np.c_[xy, prescribed][selected[witness]].tolist(),
        "scope": "Only this fixed triangulation/contacts; not global terrain impossibility.",
    }
