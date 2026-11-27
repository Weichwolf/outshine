"""Root folded grass blades in the final grass surface."""

from functools import cache

import numpy as np
from outshine_ref.vegetation.blade_surface import Blade
from plant_sites import root_elevations, scatter_mesh, tile_sites

GRASS_REACH_M = 20.0
FULL_COVER_M = 6.0


@cache
def tuft():
    vertices, faces = [], []
    for angle, bend, length in (
        (0.0, 0.8, 0.15),
        (2.4, 1.5, 0.115),
        (4.8, 1.85, 0.10),
        (1.2, 1.2, 0.14),
    ):
        mesh = Blade(
            length=length,
            width=0.014,
            widest=0.28,
            lobes=0,
            base_fill=0.23,
            bend=bend,
            fold=0.38,
            fold_power=1,
            petiole=0,
            twist=0.45,
        ).mesh(3)
        x, y, z = mesh["vertices"].T
        east = x * np.cos(angle) + z * np.sin(angle)
        north = x * np.sin(angle) - z * np.cos(angle)
        faces.append(mesh["faces"] + sum(map(len, vertices)))
        vertices.append(np.column_stack((east, north, y * 0.67)))
    return np.concatenate(vertices), np.concatenate(faces)


def blades(mesh, source, eye, seed):
    batches = list(
        tile_sites(source, eye[:2], GRASS_REACH_M, seed ^ 3103, density=78, cell_size=2)
    )
    if not batches:
        return
    xy = np.concatenate([b[0] for b in batches])
    marks = np.concatenate([b[1] for b in batches])
    heights = root_elevations(mesh, xy)
    present = np.isfinite(heights)
    anchor = np.column_stack((xy[present], heights[present] - 0.008))
    marks = marks[present]
    distance = np.linalg.norm(anchor - eye, axis=1)
    weight = np.clip((GRASS_REACH_M - distance) / (GRASS_REACH_M - FULL_COVER_M), 0, 1)
    nearby = marks[:, 2] < weight**2 * (3 - 2 * weight)
    if not nearby.any():
        return
    anchor, marks = anchor[nearby], marks[nearby]
    azimuth = marks[:, 0] * np.pi * 2
    lon, lat = (np.array([source.lon, source.lat]) + anchor[:, :2] / source.metres).T
    patch = np.sin(91400 * lon + 22800 * lat) * np.sin(27100 * lon - 83200 * lat)
    scale = 0.62 + 0.69 * marks[:, 1] ** 1.4 + 0.15 * patch
    for name, group in (
        ("grass blades", marks[:, 1] < 0.83),
        ("grass pale blades", marks[:, 1] >= 0.83),
    ):
        if group.any():
            yield (
                name,
                *scatter_mesh(tuft(), anchor[group], azimuth[group], scale[group]),
            )
