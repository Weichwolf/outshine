"""Closed intake components and a separate smoothly shaded source shell draw product."""

import numpy as np
import trimesh
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh
from outshine_ref.buildings.rooflight_panels import triangle_surface
from recipe_hall_operators import sealed


def placed_support(raw, support, material):
    start, end = np.asarray(support.start), np.asarray(support.end)
    delta = end - start
    along = delta[:2] / np.linalg.norm(delta[:2])
    across = np.array([-along[1], along[0]])
    basis = np.array(
        [
            [*along, 0],
            [*across, 0],
            [*(delta[:2] / delta[2]), 1],
        ]
    ).T
    vertices = raw["vertices"] @ basis.T + (start + end) * 0.5
    return sealed(
        triangle_surface({"vertices": vertices, "faces": raw["faces"]}, material)
    )


def beam(support, material):
    height = support.end[2] - support.start[2]
    raw = trimesh.creation.box(extents=[support.width_m, support.width_m, height])
    return placed_support(
        {"vertices": raw.vertices, "faces": raw.faces}, support, material
    )


def smooth_shell(mesh, source):
    values = np.asarray(source.knots)
    slope = np.gradient(values[:, 1], values[:, 0])
    points = mesh["vertices"]
    xy = points[:, :2] - source.centre_xy
    xy /= np.linalg.norm(xy, axis=1)[:, None]
    dr = np.interp(points[:, 2], values[:, 0], slope)
    smooth = np.c_[xy, -dr]
    smooth /= np.linalg.norm(smooth, axis=1)[:, None]
    roles = np.repeat(mesh["parts"], 3)
    smooth[roles == 1] *= -1
    normals = np.where((roles < 2)[:, None], smooth, mesh["normals"])
    return sealed(dict(mesh, normals=normals))


def components(plan, *, error_m=0.2, beam_builder=beam):
    values = {}
    for profile in (plan.shell, plan.footing, plan.collector, plan.rim):
        error = min(error_m, 0.2) if profile is plan.collector else error_m
        result = radial_mesh(profile, error_m=error)
        values[profile.identifier] = result.mesh
    for support in plan.supports:
        values[support.identifier] = beam_builder(support, plan.source.materials[0])
    return values


def draw_parts(plan, physical):
    return {
        key: smooth_shell(mesh, plan.source) if key == plan.shell.identifier else mesh
        for key, mesh in physical.items()
    }
