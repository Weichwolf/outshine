"""Root uneven grasses and wildflowers on the solved meadow surface."""

from functools import cache

import numpy as np
import shapely
from grass_cover import tuft
from ground_surface import surface_elevations
from outshine_ref.vegetation import Bellflower, Blade
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from plant_sites import patch_density, scatter_mesh, tile_sites
from scene_mesh import Meshes

MEADOW_REACH_M = 28.0


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
    return upright_products(meshes)


def upright_products(meshes):
    return {
        name: (np.asarray(vertices)[:, [0, 2, 1]] * [1, -1, 1], np.asarray(faces))
        for name, (vertices, faces) in meshes.items()
    }


@cache
def meadow_forb(cream):
    length, bend = (0.47, 0.21) if cream else (0.39, 0.32)
    stem = axis_mesh(length, bend, 0.0021, 0.0008, 4, 4)
    leaf = Blade(length=0.09, width=0.025, petiole=0, lobes=0).mesh(3)
    positions, tangent, lateral, normal = arc_frame(
        length, bend, 0, [0.26, 0.46, 0.65, 1]
    )
    meshes = Meshes()
    meshes.add("meadow stems", stem["vertices"], stem["faces"])
    for i, site in enumerate(positions[:-1]):
        heading = i * 2.4
        side = np.cos(heading) * lateral[i] + np.sin(heading) * normal[i]
        along = (tangent[i] + side) / np.sqrt(2)
        across = np.cross(side, tangent[i])
        frame = np.stack((across, along, np.cross(across, along)), axis=1)
        meshes.add("meadow blades", leaf["vertices"] @ frame.T + site, leaf["faces"])
    radius = 0.007 if cream else 0.009
    receptacle = axis_mesh(0.012, 0, 0.002, 1.16 * radius, 2, 7)
    meshes.add(
        "meadow stems",
        receptacle["vertices"] + positions[-1] - [0, 0.009, 0],
        receptacle["faces"],
    )
    flower = Bellflower(length=0.008, radius=radius, lobes=5).mesh(4, 15)
    buds = flower["vertices"][:, [0, 2, 1]] * [1, -1, 1]
    material = "meadow cream flowers" if cream else "meadow purple flowers"
    for i in range(5 if cream else 4):
        phase = i * 2.4
        offset = radius * np.array([np.sin(phase), 0.18 * (i % 2), np.cos(phase)])
        meshes.add(material, buds + positions[-1] + offset, flower["faces"])
    return upright_products(meshes)


def meadow_cover(mesh, source, eye, seed):
    batches = list(
        tile_sites(
            source, eye[:2], MEADOW_REACH_M, seed ^ 1579, density=44, cell_size=3
        )
    )
    if not batches:
        return
    xy = np.concatenate([points for points, _ in batches])
    marks = np.concatenate([values for _, values in batches])
    heights = surface_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    if not grounded.any():
        return
    xy, marks = xy[grounded], marks[grounded]
    anchors = np.column_stack((xy, heights[grounded] - 0.014))
    distance = np.linalg.norm(anchors - eye, axis=1)
    fade = np.clip((MEADOW_REACH_M - distance) / (MEADOW_REACH_M - 9), 0, 1)
    patch = patch_density(source, xy, seed)
    near = 0.5 + 0.5 * np.clip((15 - distance) / 10, 0, 1)
    active = marks[:, 2] < near * fade**2 * (3 - 2 * fade) * (0.68 + 0.32 * patch)
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
    short_scale = scale * 1.52
    for group, product, sizes in (
        (active & ~tall, tuft, short_scale),
        (active & tall, tall_tuft, scale),
    ):
        if group.any():
            geometry = product()
            radius = np.linalg.norm(geometry[0][:, :2], axis=1).max()
            group &= clearance > radius * sizes
            if not group.any():
                continue
            yield (
                "meadow blades",
                *scatter_mesh(geometry, anchors[group], angles[group], sizes[group]),
            )
    wildflowers = active & (marks[:, 0] > 0.90) & (marks[:, 1] > 0.59)
    for group, products in (
        (active & tall & (marks[:, 0] < 0.16), flowering_stalk),
        (wildflowers & (marks[:, 1] < 0.86), lambda: meadow_forb(False)),
        (wildflowers & (marks[:, 1] >= 0.86), lambda: meadow_forb(True)),
    ):
        if group.any():
            geometry = products()
            radius = max(
                np.linalg.norm(v[:, :2], axis=1).max() for v, _ in geometry.values()
            )
            group &= clearance > radius * scale
            if not group.any():
                continue
            for name, product in geometry.items():
                yield (
                    name,
                    *scatter_mesh(product, anchors[group], angles[group], scale[group]),
                )
