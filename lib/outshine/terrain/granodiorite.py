"""Rounded blocks on mapped granodiorite and loose clasts on its residuum."""

from dataclasses import dataclass
from functools import cache

import numpy as np
import shapely

from outshine.terrain.ground_surface import (
    ROUTE_MATERIALS,
    SOIL_MATERIALS,
    indexed_surface_elevations,
    surface_elevations,
)
from outshine.vegetation.open_cover import mesh_footprint, planting_surface
from outshine.vegetation.plant_sites import patch_density, tile_sites
from outshine.vegetation.woodland_sites import spaced_sites

BEDROCK = frozenset(("JOTR-GRI:Khv",))
RESIDUUM = frozenset(("JOTR-GRI:QTwrs/Khv", "JOTR-GRI:Twrs/Khv"))
OUTCROPS = frozenset(("JOTR-2012:90200",))
ROCK_MATERIAL = "granodiorite rock"
REACH_M = 195.0
PILE_SPACING_M = 8.4
RUBBLE_REACH_M = 52.0


@dataclass(frozen=True)
class RockField:
    mesh: tuple
    footprints: object


def coordinates(source, xy):
    return (np.array([source.lon, source.lat]) + xy / source.metres).T


@cache
def cube_surface(divisions):
    rings, triangles = [], []
    u, v = np.meshgrid(
        np.arange(divisions + 1), np.arange(divisions + 1), indexing="ij"
    )
    run = divisions + 1
    base = np.arange(run**2).reshape(run, run)[:-1, :-1].ravel()
    for axis in range(3):
        for positive in (False, True):
            face = np.empty((*u.shape, 3), dtype=int)
            face[..., axis] = divisions if positive else 0
            face[..., (axis + 1) % 3], face[..., (axis + 2) % 3] = u, v
            offset = len(rings) * run**2
            rings.append(face.reshape(-1, 3))
            panels = np.concatenate(
                (base[:, None] + [0, run, run + 1], base[:, None] + [0, run + 1, 1])
            )
            triangles.append(offset + (panels if positive else panels[:, ::-1]))
    vertices, lookup = np.unique(np.concatenate(rings), axis=0, return_inverse=True)
    return 2.0 * vertices / divisions - 1.0, lookup[np.concatenate(triangles)]


def rounded_block(centre, radii, yaw, rng, *, divisions):
    cube, faces = cube_surface(divisions)
    power = rng.uniform(3.0, 6.5)
    directions = cube / (
        np.sum(abs(cube) ** power, axis=1, keepdims=True) ** (1 / power)
    )
    phase = rng.uniform(0, 2 * np.pi, 3)
    grain = np.sin(
        directions @ np.array([[3.2, 2.8, 4.7], [2.3, -3.8, 3.6], [1.7, 4.4, -2.9]])
        + phase
    )
    scale = 1 + 0.083 * grain[:, 0] + 0.031 * grain[:, 1]
    vertices = radii * directions * scale[:, None]
    vertices[:, :2] += 0.095 * radii[:2] * directions[:, 2, None] * grain[:, 2, None]
    c, s = np.cos(yaw), np.sin(yaw)
    vertices[:, :2] = vertices[:, :2] @ [[c, s], [-s, c]]
    return vertices + centre, faces


def rock_sites(atlas, source, eye_xy, seed, *, rubble):
    reach = RUBBLE_REACH_M if rubble else REACH_M + PILE_SPACING_M
    candidates = list(
        tile_sites(
            source,
            eye_xy,
            reach,
            seed,
            density=0.10 if rubble else 0.0036,
            cell_size=9,
        )
    )
    if not candidates:
        return np.empty((0, 2)), np.empty((0, 3))
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    lon, lat = coordinates(source, xy)
    geology = atlas.geology_cells(lat, lon)
    bedrock = atlas.geology_mask(geology, BEDROCK)
    weathered = atlas.geology_mask(geology, RESIDUUM)
    observations, _ = atlas.observed_cells(lat, lon)
    exposed = atlas.observation_mask(observations, OUTCROPS)
    patch = patch_density(source, xy, seed ^ 0x583C, scale=4.9 if rubble else 9.0)
    if rubble:
        probability = (weathered | bedrock) * (0.19 + 0.65 * patch**1.7)
    else:
        probability = bedrock * np.where(
            exposed, 0.55 + 0.42 * patch, 0.025 + 0.18 * patch**2
        )
    selected = marks[:, 2] < probability
    return xy[selected], marks[selected]


def pile_plan(centre, mark, seed):
    rng = np.random.default_rng(int(mark[0] * 2**32) ^ seed)
    yaw = rng.uniform(0, np.pi)
    size = rng.uniform(2.6, 7.1)
    layout = np.array(
        [[-0.43, -0.41], [0.40, -0.46], [-0.40, 0.41], [0.44, 0.42], [0.0, 0.0]]
    )
    layout += rng.uniform(-0.135, 0.135, layout.shape)
    c, s = np.cos(yaw), np.sin(yaw)
    positions = centre + size * (layout @ [[c, s], [-s, c]])
    radii = size * rng.uniform(
        [0.26, 0.24, 0.19], [0.52, 0.48, 0.46], (len(positions), 3)
    )
    radii[:, 2] = np.minimum(radii[:, 2], 0.98 * radii[:, :2].max(axis=1))
    return positions, radii, yaw, rng


def anchored_blocks(positions, radii, yaw, rng, height_at, clearance, *, divisions):
    support = height_at(positions)
    widths = np.linalg.norm(radii[:, :2], axis=1) * 1.22
    points = shapely.points(positions)
    fits = shapely.contains(clearance, points) & (
        shapely.distance(clearance.boundary, points) > widths
    )
    for point, radius, width, height in zip(
        positions[fits], radii[fits], widths[fits], support[fits], strict=True
    ):
        if not np.isfinite(height):
            continue
        offsets = 0.64 * width * np.array([[1, 0], [-1, 0], [0, 1], [0, -1]])
        grades = height_at(np.vstack((point, point + offsets)))
        lowest = np.min(grades[np.isfinite(grades)])
        z = min(height + 0.39 * radius[2], lowest + 0.76 * radius[2])
        mesh = rounded_block(np.r_[point, z], radius, yaw, rng, divisions=divisions)
        yield mesh, point, radius


def piles(atlas, source, eye_xy, height_at, clearance, seed):
    xy, marks = rock_sites(atlas, source, eye_xy, seed, rubble=False)
    keep = spaced_sites(xy, marks[:, 0], PILE_SPACING_M)
    keep &= np.linalg.norm(xy - eye_xy, axis=1) < REACH_M
    for centre, mark in zip(xy[keep], marks[keep], strict=True):
        positions, radii, yaw, rng = pile_plan(centre, mark, seed)
        divisions = 8 if np.linalg.norm(centre - eye_xy) < 72 else 5
        blocks = list(
            anchored_blocks(
                positions, radii, yaw, rng, height_at, clearance, divisions=divisions
            )
        )
        if not blocks:
            continue
        for mesh, _, _ in blocks:
            yield mesh
        if len(blocks) >= 3 and rng.random() < 0.75:
            parent, point, radius = max(
                blocks, key=lambda block: block[0][0][:, 2].max()
            )
            point = point + radius[:2] * rng.uniform(-0.18, 0.18, 2)
            top = surface_elevations(parent, point[None, :])[0]
            if np.isfinite(top):
                crown = radius * rng.uniform([0.54, 0.52, 0.32], [0.75, 0.76, 0.57])
                yield rounded_block(
                    np.r_[point, top + 0.08 * crown[2]],
                    crown,
                    yaw + rng.uniform(-0.16, 0.16),
                    rng,
                    divisions=divisions,
                )


def loose_stone(atlas, source, eye_xy, height_at, clearance, footprint, seed):
    xy, marks = rock_sites(atlas, source, eye_xy, seed, rubble=True)
    if not len(xy):
        return
    radius = 0.075 + 0.56 * marks[:, 1] ** 4
    points = shapely.points(xy)
    keep = shapely.contains(clearance, points) & ~shapely.contains(footprint, points)
    keep &= shapely.distance(clearance.boundary, points) > 1.75 * radius
    xy, marks, radius = xy[keep], marks[keep], radius[keep]
    heights = height_at(xy)
    for point, mark, size, z in zip(xy, marks, radius, heights, strict=True):
        if np.isfinite(z):
            rng = np.random.default_rng(int(mark[0] * 2**32) ^ seed)
            radii = size * rng.uniform([0.83, 0.54, 0.45], [1.07, 1.01, 0.72])
            yield rounded_block(
                np.r_[point, z - 0.08 * radii[2]],
                radii,
                rng.uniform(0, np.pi),
                rng,
                divisions=3,
            )


def block_footprints(meshes):
    return shapely.union_all(
        [shapely.MultiPoint(vertices[:, :2]).convex_hull for vertices, _ in meshes]
    )


def rock_field(atlas, source, surfaces, eye_xy, seed, *, exclusion=None):
    lon, lat = coordinates(source, np.array([eye_xy - REACH_M, eye_xy + REACH_M]))
    if not any(atlas.evidence_windows((lon[0], lat[0], lon[1], lat[1]), "geology")):
        return None
    ground = planting_surface(surfaces, eye_xy, SOIL_MATERIALS, reach=REACH_M + 11)
    if ground is None:
        return None
    clearance = mesh_footprint(ground)
    routes = planting_surface(surfaces, eye_xy, ROUTE_MATERIALS, reach=REACH_M + 11)
    if routes is not None:
        clearance = clearance.difference(mesh_footprint(routes).buffer(0.28))
    if exclusion is not None:
        clearance = clearance.difference(exclusion)
    if clearance.is_empty:
        return None
    height_at = indexed_surface_elevations(ground)
    meshes = list(piles(atlas, source, eye_xy, height_at, clearance, seed ^ 0x8F31))
    occupied = block_footprints(meshes)
    rubble = list(
        loose_stone(
            atlas, source, eye_xy, height_at, clearance, occupied, seed ^ 0x25C6
        )
    )
    meshes.extend(rubble)
    if not meshes:
        return None
    vertices, faces = zip(*meshes, strict=True)
    starts = np.r_[0, np.cumsum(list(map(len, vertices)))[:-1]]
    joined = (
        np.concatenate(vertices),
        np.concatenate(
            [part + first for part, first in zip(faces, starts, strict=True)]
        ),
    )
    return RockField(joined, occupied.union(block_footprints(rubble)))
