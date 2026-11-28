"""Root uneven grasses and wildflowers on the solved meadow surface."""

from functools import cache

import numpy as np
import shapely
from ground_surface import MEADOW_MARGIN_M, margin_weights, surface_elevations
from outshine_ref.vegetation import Bellflower, Blade
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from plant_sites import patch_density, scatter_mesh, tile_sites
from scene_mesh import Meshes

MEADOW_REACH_M = 28.0
FULL_MEADOW_COVER_M = 11.0
MEADOW_CANDIDATES_M2 = 52.0
MIN_COARSE_HEIGHT_PX = 2.0


@cache
def meadow_tuft(tall):
    seed, count, shape = (
        (557, 11, (2.0, 2.0, 1.0)) if tall else (585, 6, (2.15, 2.15, 0.56))
    )
    mesh = graminoid_tuft_mesh(seed, blades=count, segments=2)
    vertices = mesh["vertices"] * shape
    u, t = mesh["uv"].T
    edges = (u != 0.5) | (t == 1)
    flat = np.array(((0, 1, 3), (0, 3, 2), (2, 3, 4)), dtype=np.int32)
    faces = (flat + 5 * np.arange(count, dtype=np.int32)[:, None, None]).reshape(-1, 3)
    left, crease, right = vertices[t < 1].reshape(-1, 3, 3).transpose(1, 0, 2)
    fold = float(np.linalg.norm(crease - (left + right) / 2, axis=1).max())
    return (vertices, mesh["faces"]), (vertices[edges], faces), fold


@cache
def coarse_tuft():
    _, (vertices, faces), _ = meadow_tuft(False)
    leaves = vertices.reshape(-1, 5, 3)[::2].copy()
    sides = leaves[:, :4].reshape(len(leaves), 2, 2, 3)
    centre = sides.mean(axis=2, keepdims=True)
    sides[:] = centre + 2 * (sides - centre)
    triangles = faces[:3] + 5 * np.arange(len(leaves), dtype=np.int32)[:, None, None]
    return leaves.reshape(-1, 3) * (1.45, 1.45, 1.0), triangles.reshape(-1, 3)


@cache
def flowering_stalk():
    length, bend = 0.57, 0.49
    stem = axis_mesh(length, bend, 0.0016, 0.00055, 3, 3)
    grain = axis_mesh(0.044, 0.19, 0.005, 0.0012, 2, 3)
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
    stem = axis_mesh(length, bend, 0.0021, 0.0008, 3, 3)
    leaf = Blade(length=0.09, width=0.025, petiole=0, lobes=0).mesh(2)
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
    radius = 0.008 if cream else 0.011
    receptacle = axis_mesh(0.012, 0, 0.002, 1.16 * radius, 2, 3)
    meshes.add(
        "meadow stems",
        receptacle["vertices"] + positions[-1] - [0, 0.009, 0],
        receptacle["faces"],
    )
    sides = np.arange(15)
    rim = Bellflower(length=0.008, radius=radius, lobes=5).surface(
        1, 2 * np.pi * sides / len(sides)
    )[0]
    rim[:, :2] *= 1.4
    buds = np.vstack((rim, np.zeros(3)))[:, [0, 2, 1]] * [1, -1, 1]
    petals = np.column_stack(
        (np.full_like(sides, len(sides)), sides, (sides + 1) % len(sides))
    )
    material = "meadow cream flowers" if cream else "meadow purple flowers"
    for i in range(5 if cream else 4):
        phase = i * 2.4
        offset = radius * np.array([np.sin(phase), 0.18 * (i % 2), np.cos(phase)])
        meshes.add(material, buds + positions[-1] + offset, petals)
    return upright_products(meshes)


def meadow_cover(mesh, source, eye, seed, *, focal_pixels, woodland_edges=None):
    distant = coarse_tuft()
    distant_height = distant[0][:, 2].max()
    reach = max(MEADOW_REACH_M, distant_height * focal_pixels / MIN_COARSE_HEIGHT_PX)
    batches = []
    for radius, density, salt, coarse in (
        (MEADOW_REACH_M, MEADOW_CANDIDATES_M2, 1579, False),
        (reach, 4.0, 0x5109, True),
    ):
        batches.extend(
            (points, marks, np.full(len(points), coarse))
            for points, marks in tile_sites(
                source, eye[:2], radius, seed ^ salt, density=density, cell_size=3
            )
        )
    if not batches:
        return
    xy, marks, coarse = (np.concatenate(rows) for rows in zip(*batches, strict=True))
    heights = surface_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    if not grounded.any():
        return
    xy, marks, coarse = xy[grounded], marks[grounded], coarse[grounded]
    anchors = np.column_stack((xy, heights[grounded] - 0.014))
    distance = np.linalg.norm(anchors - eye, axis=1)
    pixels_per_metre = focal_pixels / np.maximum(distance, 0.05)
    fade = np.clip(
        (MEADOW_REACH_M - distance) / (MEADOW_REACH_M - FULL_MEADOW_COVER_M), 0, 1
    )
    patch = patch_density(source, xy, seed)
    cover = np.ones_like(patch)
    if woodland_edges is not None:
        margin = margin_weights(woodland_edges, xy, MEADOW_MARGIN_M)
        cover -= 0.85 * margin**2 * (3 - 2 * margin)
    active = ~coarse & (
        marks[:, 2] < fade**2 * (3 - 2 * fade) * (0.68 + 0.32 * patch) * cover
    )
    tall = marks[:, 1] < (0.15 + 0.55 * patch**2) * cover
    scale = 0.58 + 0.90 * np.mod(7 * marks[:, 0], 1) + 0.37 * patch
    angles = 2 * np.pi * np.mod(13 * marks[:, 0], 1)

    vertices, faces = mesh
    triangles = np.asarray(vertices)[np.asarray(faces), :2]
    triangles = triangles[
        np.all(triangles.max(axis=1) >= eye[:2] - reach - 1, axis=1)
        & np.all(triangles.min(axis=1) <= eye[:2] + reach + 1, axis=1)
    ]
    floor = shapely.union_all(shapely.polygons(triangles))
    clearance = shapely.distance(floor.boundary, shapely.points(xy))
    for stature in (False, True):
        folded, flat, fold = meadow_tuft(stature)
        radius = np.linalg.norm(folded[0][:, :2], axis=1).max()
        fitted = scale if stature else np.minimum(scale, clearance / radius)
        group = (
            active
            & (tall == stature)
            & ((clearance >= radius * scale) if stature else True)
            & (folded[0][:, 2].max() * fitted * pixels_per_metre >= 1)
        )
        fold_pixels = fold * fitted * pixels_per_metre
        detailed = 0.5 + 0.25 * marks[:, 0] < fold_pixels
        for selected, geometry in (
            (group & detailed, folded),
            (group & ~detailed, flat),
        ):
            if selected.any():
                yield (
                    "meadow blades",
                    *scatter_mesh(
                        geometry, anchors[selected], angles[selected], fitted[selected]
                    ),
                )
    flower_patch = patch_density(source, xy, seed ^ 0x4143, scale=0.65)
    flower_cover = 0.07 + 0.26 * flower_patch**2
    wildflowers = (
        active & (marks[:, 0] > 1 - flower_cover * cover) & (marks[:, 1] > 0.38)
    )
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
    enter = np.clip((distance - 16) / (MEADOW_REACH_M - 16), 0, 1)
    leave = np.clip((reach - distance) / max(reach * 0.30, 1), 0, 1)
    coverage = enter * enter * (3 - 2 * enter) * leave * leave * (3 - 2 * leave) * cover
    radius = np.linalg.norm(distant[0][:, :2], axis=1).max()
    fitted = np.minimum(scale, clearance / radius)
    selected = (
        coarse
        & (marks[:, 2] < coverage * (0.48 + 0.42 * patch))
        & (distant_height * fitted * pixels_per_metre >= MIN_COARSE_HEIGHT_PX)
    )
    if selected.any():
        yield (
            "meadow blades",
            *scatter_mesh(
                distant, anchors[selected], angles[selected], fitted[selected]
            ),
        )
