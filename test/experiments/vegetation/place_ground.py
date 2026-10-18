"""Actual curved DEM context; source heights remain unchanged."""

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.places import tile_geography


def terrain(camera, elevation, radius_m):
    if not 20000 <= radius_m <= 200000:
        raise ValueError("bounded actual-source terrain radius required")
    positive = np.unique(
        np.concatenate(
            (
                np.arange(0, 17, 1),
                np.arange(16, 129, 8),
                np.arange(128, 4097, 64),
                np.arange(4096, 20001, 500),
                np.arange(20000, radius_m, 2000),
                (radius_m,),
            )
        )
    )
    offsets = np.concatenate((-positive[:0:-1], positive))
    cosine = np.cos(np.deg2rad(camera.latitude_deg))
    world_offsets = offsets / (40075016.68557849 * cosine)
    center = np.array(
        (
            (camera.longitude_deg + 180) / 360,
            (1 - np.arcsinh(np.tan(np.deg2rad(camera.latitude_deg))) / np.pi) / 2,
        )
    )
    x, y = np.meshgrid(center[0] + world_offsets, center[1] + world_offsets)
    latitude, longitude = tile_geography(
        np.column_stack((x.ravel(), y.ravel())), 1, (0, 0, 0)
    )
    heights = elevation.sample(latitude, longitude)
    vertices = camera.local(latitude, longitude, heights)
    side = len(offsets)
    a = (np.arange(side - 1)[:, None] * side + np.arange(side - 1)[None, :]).ravel()
    faces = np.concatenate(
        (
            np.column_stack((a, a + side, a + 1)),
            np.column_stack((a + 1, a + side, a + side + 1)),
        )
    ).astype(np.uint32)
    model = Mesh(vertices, faces)
    # The initial planting window is local. Do not index remote mountain faces.
    bounds = np.max(abs(vertices[faces, :2]), axis=(1, 2))
    local = faces[bounds <= 12000]
    sampler = SurfaceSampler(Mesh(vertices, local), max_faces=250000)
    return (
        model,
        sampler,
        {
            "vertices": len(vertices),
            "triangles": len(faces),
            "radius_m": radius_m,
            "local_contact_triangles": len(local),
            "height_range_m": [float(heights.min()), float(heights.max())],
            "limits": "Variable regular source grid, not a terrain LOD admission. Heights use finest declared DEM per sample; source resolution boundaries may remain visible. Curvature retained; no geoid conversion.",
        },
    )
