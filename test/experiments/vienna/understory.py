"""Generate fern and tall-grass patches on exposed woodland ground."""

from functools import cache

import numpy as np
import shapely
from outshine_ref.vegetation import FrondGuide, PinnatePattern
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from plant_sites import root_elevations, scatter_mesh, tile_sites
from scipy.spatial.transform import Rotation

UNDERSTORY_REACH_M = 35.0


@cache
def fern():
    vertices, faces = [], []
    start = 0
    for i in range(7):
        length = 0.64 + 0.11 * np.sin(3 * i + 0.7)
        bend = -1.13
        pattern = PinnatePattern(
            seed=31 + i, length=length, pairs=11, leaflet_length=0.24, aspect=0.29
        )
        frond = FrondGuide(length=length, bend=bend, fold_per_m=1.2).deform(
            pattern.mesh(5, 1)
        )
        rachis = axis_mesh(length, bend, 0.0033, 0.0007, 7, 5)
        heading, lean = i * 2.4, 0.95 + 0.23 * np.sin(i * 4.1)
        rotation = Rotation.from_euler("ZX", (heading, lean))
        for part in (frond, rachis):
            vertices.append(rotation.apply(part["vertices"]))
            faces.append(part["faces"] + start)
            start += len(part["vertices"])
    return np.concatenate(vertices), np.concatenate(faces)


@cache
def tall_grass():
    mesh = graminoid_tuft_mesh(55, blades=19, segments=4)
    return mesh["vertices"], mesh["faces"]


def woodland_cover(mesh, source, domain, eye, seed):
    if domain is None:
        return
    centre = eye[:2]
    window = shapely.Point(centre).buffer(UNDERSTORY_REACH_M)
    planting = domain.intersection(window.buffer(1)).buffer(-0.72)
    if planting.is_empty:
        return
    sites = list(
        tile_sites(
            source, centre, UNDERSTORY_REACH_M, seed ^ 763981, density=1.35, cell_size=4
        )
    )
    if not sites:
        return
    xy = np.concatenate([b[0] for b in sites])
    marks = np.concatenate([b[1] for b in sites])
    inside = shapely.contains_xy(planting, *xy.T)
    xy, marks = xy[inside], marks[inside]
    heights = root_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    xy, marks = xy[grounded], marks[grounded]
    anchors = np.column_stack((xy, heights[grounded] - 0.018))

    lon, lat = (np.array([source.lon, source.lat]) + xy / source.metres).T
    patches = np.sin(28200 * lon + 23300 * lat) * np.sin(12600 * lon - 52200 * lat)
    fern_cover = np.maximum(patches, 0)
    reach = np.linalg.norm(anchors - eye, axis=1)
    coverage = np.clip((UNDERSTORY_REACH_M - reach) / 12, 0, 1)
    scales = 0.62 + 0.56 * marks[:, 1]
    angles = 2 * np.pi * marks[:, 0]
    for name, group, plant in (
        ("woodland ferns", marks[:, 2] < coverage * (0.07 + 0.57 * fern_cover), fern),
        (
            "woodland grasses",
            1 - marks[:, 2] < coverage * (0.55 - 0.32 * fern_cover),
            tall_grass,
        ),
    ):
        if group.any():
            yield (
                name,
                *scatter_mesh(plant(), anchors[group], angles[group], scales[group]),
            )
