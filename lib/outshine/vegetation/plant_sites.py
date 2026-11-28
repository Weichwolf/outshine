"""Sample tile-stable plant sites and root their geometry in the solved ground."""

import numpy as np

from outshine.places.sources import tile_geography
from outshine.vegetation.ground_cover import GroundCoverPlan


def patch_density(source, xy, seed, *, scale=1.0):
    longitude_latitude = np.array([source.lon, source.lat]) + xy / source.metres
    uv = longitude_latitude * (np.array([11600.0, 20000.0]) / scale)
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


def scatter_mesh(mesh, anchors, angles, scales):
    vertices, faces = mesh
    x, y, z = vertices.T
    c, s = np.cos(angles[:, None]), np.sin(angles[:, None])
    rotated = np.stack(
        (c * x - s * y, s * x + c * y, np.broadcast_to(z, (len(anchors), len(z)))),
        axis=-1,
    )
    points = rotated * scales[:, None, None] + anchors[:, None, :]
    offsets = np.arange(len(anchors), dtype=np.int32) * len(vertices)
    return points.reshape(-1, 3), (faces + offsets[:, None, None]).reshape(-1, 3)


def tile_sites(source, centre, radius, seed, *, density, cell_size, receipts=None):
    anchor = np.array([source.lon, source.lat])
    lower = anchor + (centre - radius) / source.metres
    upper = anchor + (centre + radius) / source.metres
    for receipt in source.receipts if receipts is None else receipts:
        if receipt["kind"] != "vector":
            continue
        tile = receipt["tile"]
        lat, lon = tile_geography([[0, 1], [1, 0]], 1, tile)
        sw, ne = np.column_stack((lon, lat))
        metres = np.array([111319.49 * np.cos(np.deg2rad(lat.mean())), 111132.0])
        lo, hi = np.maximum(sw, lower), np.minimum(ne, upper)
        if np.any(hi <= lo):
            continue
        plan = GroundCoverPlan(
            seed ^ (tile[1] << 24) ^ tile[2], density_m2=density, cell_m=cell_size
        )
        sites = plan.sites((lo - sw) * metres, (hi - sw) * metres, max_cells=65536)
        if sites is None:
            raise RuntimeError(f"plant site query exceeds budget for tile {tile}")
        xy = (sw - anchor + sites["xy"] / metres) * source.metres
        keep = np.sum((xy - centre) ** 2, axis=1) < radius**2
        if keep.any():
            yield xy[keep], sites["marks"][keep]
