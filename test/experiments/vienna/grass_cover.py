"""Root folded grass blades in the final grass surface."""

from functools import cache

import numpy as np
import shapely
from outshine_ref.vegetation.blade_surface import Blade
from plant_sites import tile_sites

GRASS_REACH_M = 20.0
FULL_COVER_M = 6.0


def root_elevations(mesh, xy):
    vertices, faces = mesh
    if not len(faces):
        return np.full(len(xy), -np.inf)
    triangles = np.asarray(vertices)[np.asarray(faces)]
    footprints = shapely.polygons(triangles[..., :2])
    valid = shapely.area(footprints) > 1e-10
    triangles = triangles[valid]
    site, triangle = shapely.STRtree(footprints[valid]).query(
        shapely.points(xy), predicate="covered_by"
    )
    origin, east, north = triangles[triangle].transpose(1, 0, 2)
    east = east - origin
    north = north - origin
    delta = xy[site] - origin[:, :2]
    det = east[:, 0] * north[:, 1] - east[:, 1] * north[:, 0]
    weight_east = (delta[:, 0] * north[:, 1] - delta[:, 1] * north[:, 0]) / det
    weight_north = (east[:, 0] * delta[:, 1] - east[:, 1] * delta[:, 0]) / det
    heights = np.full(len(xy), -np.inf)
    np.maximum.at(
        heights,
        site,
        origin[:, 2] + east[:, 2] * weight_east + north[:, 2] * weight_north,
    )
    return heights


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
    vertices, faces = tuft()
    x, y, z = vertices.T
    c, s = np.cos(azimuth[:, None]), np.sin(azimuth[:, None])
    rotated = np.stack(
        (c * x - s * y, s * x + c * y, np.broadcast_to(z, (len(anchor), len(z)))),
        axis=-1,
    )
    positions = rotated * scale[:, None, None] + anchor[:, None, :]
    for name, group in (
        ("grass blades", marks[:, 1] < 0.83),
        ("grass pale blades", marks[:, 1] >= 0.83),
    ):
        if group.any():
            offsets = np.arange(np.count_nonzero(group), dtype=np.int32) * len(vertices)
            yield (
                name,
                positions[group].reshape(-1, 3),
                (faces + offsets[:, None, None]).reshape(-1, 3),
            )
