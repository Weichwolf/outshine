"""World-tile plant identities; bounded near grass is a demand window, not a plan."""

import numpy as np
from outshine_ref.places import tile_geography
from outshine_ref.vegetation import species_site_uniforms


def candidates(camera, tile):
    rng = np.random.default_rng(np.random.SeedSequence((17, *tile)))
    side = 192
    x, y = np.meshgrid(np.arange(side), np.arange(side))
    cells = np.column_stack((x.ravel(), y.ravel()))
    pixels = (cells + 0.5 + rng.uniform(-0.38, 0.38, cells.shape)) * (4096 / side)
    base_count = len(cells)
    sparse_grass = rng.random(base_count) < 0.035
    # Fine cells are world-anchored MVT pixels. Enlarging or turning the demand
    # window never reseeds a shared cell. Offset identities separate both grids.
    latitude = np.deg2rad(camera.latitude_deg)
    center = (
        np.array(
            (
                (camera.longitude_deg + 180) / 360 * 2**14 - tile[1],
                (1 - np.arcsinh(np.tan(latitude)) / np.pi) / 2 * 2**14 - tile[2],
            )
        )
        * 4096
    )
    margin = 40 / (40075016.68557849 * np.cos(latitude) / 2**14 / 4096) + 2
    lower = np.maximum(0, np.floor(center - margin)).astype(int)
    upper = np.minimum(4096, np.ceil(center + margin)).astype(int)
    x, y = np.meshgrid(np.arange(lower[0], upper[0]), np.arange(lower[1], upper[1]))
    fine = np.column_stack((x.ravel(), y.ravel()))
    fine_keys = np.column_stack((np.tile(tile, (len(fine), 1)), fine + 4096))
    jitter = np.column_stack(
        (species_site_uniforms(fine_keys, 7041), species_site_uniforms(fine_keys, 7042))
    )
    fine_pixels = fine + 0.5 + (jitter - 0.5) * 0.76
    cells = np.concatenate((cells, fine + 4096))
    latitude, longitude = tile_geography(
        np.concatenate((pixels, fine_pixels)), 4096, tile
    )
    keys = np.column_stack((np.tile(tile, (len(cells), 1)), cells))
    return {
        "latitude": latitude,
        "longitude": longitude,
        "ids": keys,
        "tree": np.arange(len(cells)) < base_count,
        "grass": np.concatenate((sparse_grass, np.ones(len(fine), dtype=bool))),
        "angle": species_site_uniforms(keys, 7043) * (2 * np.pi),
        "scale": 0.8 + 0.8 * species_site_uniforms(keys, 7044),
    }
