"""Root uneven grasses and seed heads on the solved meadow surface."""

from functools import cache

import numpy as np
import shapely
from grass_cover import tuft
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from plant_sites import root_elevations, scatter_mesh, tile_sites
from scene_mesh import Meshes

MEADOW_REACH_M = 28.0


def patch_density(source, xy, seed):
    longitude_latitude = np.array([source.lon, source.lat]) + xy / source.metres
    uv = longitude_latitude * [11600.0, 20000.0]
    cells = np.floor(uv).astype(np.int64)
    weight = uv - cells
    weight = weight**2 * (3 - 2 * weight)
    corners = (cells[:, None, :] + [[0, 0], [1, 0], [0, 1], [1, 1]]).astype(np.uint32)
    x, y = corners[..., 0], corners[..., 1]
    bits = (
        x * np.uint32(0x9E3779B9)
        ^ y * np.uint32(0x85EBCA6B)
        ^ np.uint32(seed & 0xFFFFFFFF)
    )
    bits = (bits ^ (bits >> 16)) * np.uint32(0x7FEB352D)
    bits = (bits ^ (bits >> 15)) * np.uint32(0x846CA68B)
    a, b, c, d = ((bits ^ (bits >> 16)) / 2**32).T
    u, v = weight.T
    return ((1 - u) * a + u * b) * (1 - v) + ((1 - u) * c + u * d) * v


@cache
def tall_tuft():
    mesh = graminoid_tuft_mesh(557, blades=9, segments=3)
    vertices = mesh["vertices"] * [1.65, 1.65, 0.87]
    return vertices, mesh["faces"]


@cache
def flowering_stalk():
    length, bend = 0.57, 0.49
    stem = axis_mesh(length, bend, 0.0016, 0.00055, 5, 4)
    grain = axis_mesh(0.044, 0.19, 0.005, 0.0012, 2, 5)
    t = np.linspace(0.81, 0.98, 5)
    sites, tangent, lateral, normal = arc_frame(length, bend, 0, t)
    meshes = Meshes()
    meshes.add("meadow stems", stem["vertices"], stem["faces"])
    for index, site in enumerate(sites):
        angle = 2.4 * index
        outward = np.cos(angle) * lateral[index] + np.sin(angle) * normal[index]
        along = 0.940 * tangent[index] + 0.341 * outward
        side = np.cross(outward, tangent[index])
        frame = np.stack((side, along, np.cross(side, along)), axis=1)
        meshes.add("meadow seeds", grain["vertices"] @ frame.T + site, grain["faces"])
    return {
        name: (np.asarray(vertices)[:, [0, 2, 1]] * [1, -1, 1], np.asarray(faces))
        for name, (vertices, faces) in meshes.items()
    }


def meadow_cover(mesh, source, eye, seed):
    batches = list(
        tile_sites(
            source, eye[:2], MEADOW_REACH_M, seed ^ 1579, density=22, cell_size=3
        )
    )
    if not batches:
        return
    xy = np.concatenate([points for points, _ in batches])
    marks = np.concatenate([values for _, values in batches])
    heights = root_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    if not grounded.any():
        return
    xy, marks = xy[grounded], marks[grounded]
    anchors = np.column_stack((xy, heights[grounded] - 0.014))
    distance = np.linalg.norm(anchors - eye, axis=1)
    fade = np.clip((MEADOW_REACH_M - distance) / (MEADOW_REACH_M - 9), 0, 1)
    patch = patch_density(source, xy, seed)
    active = marks[:, 2] < fade**2 * (3 - 2 * fade) * (0.65 + 0.35 * patch)
    tall = marks[:, 1] < 0.15 + 0.55 * patch**2
    scale = 0.58 + 0.90 * np.mod(7 * marks[:, 0], 1) + 0.37 * patch
    angles = 2 * np.pi * marks[:, 0]

    vertices, faces = mesh
    triangles = np.asarray(vertices)[np.asarray(faces), :2]
    triangles = triangles[
        np.all(triangles.max(axis=1) >= eye[:2] - MEADOW_REACH_M - 1, axis=1)
        & np.all(triangles.min(axis=1) <= eye[:2] + MEADOW_REACH_M + 1, axis=1)
    ]
    floor = shapely.union_all(shapely.polygons(triangles))
    clearance = shapely.distance(floor.boundary, shapely.points(xy))
    for group, product in ((active & ~tall, tuft), (active & tall, tall_tuft)):
        if group.any():
            geometry = product()
            radius = np.linalg.norm(geometry[0][:, :2], axis=1).max()
            group &= clearance > radius * scale
            if not group.any():
                continue
            yield (
                "meadow blades",
                *scatter_mesh(geometry, anchors[group], angles[group], scale[group]),
            )
    seeds = active & tall & (marks[:, 0] < 0.21)
    if seeds.any():
        stalk = flowering_stalk()
        radius = max(np.linalg.norm(v[:, :2], axis=1).max() for v, _ in stalk.values())
        seeds &= clearance > radius * scale
        if seeds.any():
            for name, product in stalk.items():
                yield (
                    name,
                    *scatter_mesh(product, anchors[seeds], angles[seeds], scale[seeds]),
                )
