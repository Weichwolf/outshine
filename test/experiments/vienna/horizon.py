"""Terrain beyond the local square, sampled in WGS84 and placed in the eye's ENU frame."""

from concurrent.futures import ThreadPoolExecutor

import numpy as np
from outshine_ref.places.camera import axes, ecef
from outshine_ref.places.height import Elevation
from outshine_ref.places.sources import acquire

HORIZON_RANGE_M = 100_000


def terrain_horizon(source, cache, radius):
    edge = np.linspace(-1, 1, 129)[:-1]
    side = np.ones_like(edge)
    perimeter = np.concatenate(
        [
            np.c_[edge, -side],
            np.c_[side, edge],
            np.c_[-edge, side],
            np.c_[-side, -edge],
        ]
    )
    radii = np.geomspace(radius, HORIZON_RANGE_M, 240)
    xy = radii[:, None, None] * perimeter
    lon, lat = (xy.reshape(-1, 2) / source.metres + [source.lon, source.lat]).T
    world = np.column_stack(
        ((lon + 180) / 360, (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2)
    )
    zoom = np.repeat(
        np.clip(
            np.ceil(
                np.log2(
                    40075016.7
                    * np.cos(np.deg2rad(source.lat))
                    / (256 * np.maximum(1, radii / 96))
                )
            ),
            7,
            12,
        ).astype(int),
        len(perimeter),
    )
    cells = np.floor(world * (2 ** zoom[:, None])).astype(int)
    tiles = np.unique(np.column_stack((zoom, cells)), axis=0).tolist()

    def load(tile):
        url = "https://tiles.mapterhorn.com/" + "/".join(map(str, tile)) + ".webp"
        return acquire(("elevation", tile, url), cache, fetch=True)

    with ThreadPoolExecutor(max_workers=6) as workers:
        receipts = list(workers.map(load, tiles))
    dem = Elevation(receipts)
    heights = dem.sample(lat, lon)
    anchor = ecef(source.lat, source.lon, source.datum)
    vertices = (ecef(lat, lon, heights) - anchor) @ axes(source.lat, source.lon).T
    count = len(perimeter)
    # Even vertices match the inner grid. Odd vertices subdivide its existing edges.
    anchor_z = source.height(xy[0, ::2])
    vertices[:count, :2] = xy[0]
    vertices[:count:2, 2] = anchor_z
    vertices[1:count:2, 2] = (anchor_z + np.roll(anchor_z, -1)) * 0.5
    a = np.arange((len(radii) - 1) * count)
    b = a - a % count + (a + 1) % count
    triangles = np.column_stack((a, b + count, b, a, a + count, b + count)).reshape(
        -1, 3
    )
    return vertices, triangles, receipts
