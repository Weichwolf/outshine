"""Tileable PBR detail shared by the reference renderers, in metre units."""

from functools import cache

import numpy as np
from scipy.ndimage import gaussian_filter, map_coordinates

DETAIL_SPAN_M = 2.56
MACRO_SPAN_M = 23.0
HEIGHT_SPAN_M = 0.024
BARK_BOX_BLEND = 0.35
SIZE = 512


SURFACE_DETAIL = {
    "road": "asphalt",
    "paving": "stone",
    "bridge deck": "concrete",
    "sidewalk": "concrete",
    "stair limestone": "concrete",
    "foundation": "concrete",
    "ground": "soil",
    "unpaved path": "compacted",
    "wall": "plaster",
    "oak bark": "oak bark",
    "beech bark": "beech bark",
    "foliage": "leaf",
}


def noise(rng, scale):
    signal = gaussian_filter(rng.standard_normal((SIZE, SIZE)), scale, mode="wrap")
    return np.clip(signal / signal.std(), -3, 3)


def stone_setts(rng, grain):
    rows_per_tile, columns_per_tile = 20, 12
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    y *= rows_per_tile / SIZE
    x *= columns_per_tile / SIZE
    y += 0.042 * noise(rng, 10)
    rows = np.floor(y).astype(int) % rows_per_tile
    row_shift = rng.uniform(-0.12, 0.12, rows_per_tile)
    x += 0.5 * (rows & 1) + row_shift[rows] + 0.055 * noise(rng, 9)
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    edge = np.minimum.reduce([x - ix, y - iy, 1 - x + ix, 1 - y + iy])
    face = np.clip((edge - 0.025) / 0.05, 0, 1)
    stones = rng.uniform(-1, 1, (rows_per_tile, columns_per_tile))[
        iy % rows_per_tile, ix % columns_per_tile
    ]
    return (
        0.68 + (0.30 + 0.09 * stones) * face + 0.035 * grain,
        0.90 - (0.15 + 0.035 * stones) * face + 0.025 * grain,
        (0.0032 + 0.0010 * stones) * face + 0.0004 * grain,
    )


@cache
def ground_detail(name):
    seed = (
        "asphalt",
        "stone",
        "concrete",
        "soil",
        "compacted",
        "plaster",
        "oak bark",
        "leaf",
        "beech bark",
    ).index(name)
    rng = np.random.default_rng(seed)
    fine, grain, patches = (noise(rng, scale) for scale in (0.65, 1.6, 12))
    if name == "asphalt":
        aggregate = np.maximum(fine - 0.40, 0)
        tint = 0.97 + 0.18 * aggregate + 0.045 * grain
        roughness = 0.86 - 0.04 * aggregate + 0.018 * patches
        height = 0.0011 * aggregate + 0.00032 * grain
    elif name == "stone":
        tint, roughness, height = stone_setts(rng, fine)
    elif name == "concrete":
        pits = np.maximum(-fine - 1.35, 0)
        tint = 1 - 0.12 * pits + 0.045 * grain
        roughness = 0.77 + 0.045 * pits + 0.025 * patches
        height = -0.0010 * pits + 0.00030 * grain
    elif name == "compacted":
        aggregate = np.maximum(grain - 0.75, 0)
        tint = 1 + 0.075 * aggregate + 0.04 * fine + 0.05 * patches
        roughness = 0.89 - 0.035 * aggregate + 0.018 * patches
        height = 0.0014 * aggregate + 0.00035 * fine + 0.00045 * patches
    elif name == "soil":
        humus = noise(rng, 30)
        litter = np.maximum(noise(rng, (2.5, 0.95)) - 0.7, 0)
        tint = 0.97 + 0.14 * litter + 0.055 * grain + 0.075 * humus
        roughness = 0.92 - 0.065 * litter + 0.025 * patches
        height = 0.0025 * litter + 0.00070 * fine + 0.0021 * humus
    elif name == "plaster":
        pits = np.maximum(-fine - 0.9, 0)
        trowel = noise(rng, (9, 19))
        tint = 1 - 0.045 * pits + 0.022 * grain + 0.026 * trowel
        roughness = 0.83 + 0.028 * pits + 0.024 * trowel
        height = -0.00060 * pits + 0.00035 * grain + 0.00060 * trowel
    elif name == "oak bark":
        y, x = np.indices((SIZE, SIZE), dtype=np.float32)
        shift = 3.5 * noise(rng, (18, 6))
        grain_flow = map_coordinates(
            noise(rng, (20, 1.9)), [y, x + shift], order=1, mode="grid-wrap"
        )
        plates = noise(rng, (6, 2.9))
        fissures = np.maximum(-grain_flow + 0.4 * plates - 0.40, 0)
        tint = 1 - 0.24 * fissures + 0.075 * patches + 0.055 * plates
        roughness = 0.90 + 0.032 * fissures + 0.019 * grain
        height = -0.0040 * fissures + 0.0012 * plates + 0.00034 * fine
    elif name == "beech bark":
        mottling = noise(rng, (22, 15))
        scars = np.maximum(noise(rng, (1.8, 8)) - 1.55, 0)
        tint = 1 + 0.07 * mottling + 0.040 * patches - 0.17 * scars
        roughness = 0.84 + 0.030 * mottling + 0.022 * patches
        height = -0.0014 * scars + 0.00045 * patches + 0.00024 * grain
    else:  # Leaf tissue variation in the shared foliage batch.
        tint = 1 + 0.065 * grain + 0.15 * patches
        roughness = 0.57 + 0.04 * grain + 0.035 * patches
        height = 0.00016 * grain
    pixels = np.stack(
        (
            np.clip(tint * 0.5, 0, 1),
            np.clip(roughness, 0.05, 1),
            np.clip(0.5 + height / HEIGHT_SPAN_M, 0, 1),
            np.clip(0.5 + 0.15 * patches, 0, 1),
        ),
        axis=-1,
    ).astype("f4")
    if name in ("leaf", "beech bark"):
        half = SIZE // 2
        return pixels.reshape(half, 2, half, 2, 4).mean(axis=(1, 3))
    return pixels
