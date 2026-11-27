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
    "retaining wall": "concrete",
    "sidewalk": "concrete",
    "stair limestone": "concrete",
    "foundation": "concrete",
    "portal stone": "concrete",
    "window sill": "concrete",
    "ground": "soil",
    "grass": "grass",
    "meadow": "meadow",
    "unpaved path": "compacted",
    "wall": "plaster",
    "oak bark": "oak bark",
    "beech bark": "beech bark",
    "foliage": "leaf",
    "weir overflow": "weir overflow",
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
        "grass",
        "beech bark",
        "weir overflow",
        "meadow",
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
        shift = 7 * noise(rng, (20, 8)) + 1.8 * noise(rng, (5, 3))
        grain_flow = map_coordinates(
            noise(rng, (16, 2.5)), [y, x + shift], order=1, mode="grid-wrap"
        )
        plates = noise(rng, (7, 4))
        breaks = np.maximum(-noise(rng, (1.6, 5)) + 0.35 * plates - 1.12, 0)
        fissures = np.maximum(-grain_flow + 0.56 * plates - 0.25, 0)
        tint = 1 - 0.26 * fissures - 0.15 * breaks + 0.15 * patches + 0.080 * plates
        roughness = 0.90 + 0.032 * fissures + 0.019 * grain
        height = -0.0044 * fissures - 0.0028 * breaks + 0.0015 * plates + 0.00046 * fine
    elif name == "beech bark":
        mottling = noise(rng, (28, 21))
        scars = np.maximum(noise(rng, (2.2, 8)) - 1.29, 0)
        lichen = np.clip(noise(rng, (16, 10)) + 0.35 * grain - 0.10, 0, 1)
        tint = 0.90 + 0.24 * mottling + 0.25 * lichen + 0.055 * patches - 0.28 * scars
        roughness = 0.84 + 0.030 * mottling + 0.015 * lichen + 0.022 * patches
        height = -0.0017 * scars + 0.00051 * patches + 0.00028 * grain
    elif name == "weir overflow":
        threads = noise(rng, (15, 1.5))
        air = np.clip(0.5 + 0.24 * threads + 0.12 * grain, 0, 1)
        tint = 0.62 + 1.0 * air
        roughness = 0.23 + 0.29 * air
        height = 0.0012 * threads + 0.00055 * fine
    elif name == "grass":
        clumps = noise(rng, (3.6, 1.8))
        tips = np.maximum(fine - 0.45, 0)
        tint = 1 + 0.12 * clumps + 0.07 * grain - 0.10 * tips
        roughness = 0.90 + 0.030 * clumps - 0.020 * tips
        height = 0.0022 * clumps + 0.0008 * tips
    elif name == "meadow":
        clumps = noise(rng, (6, 4))
        litter = np.maximum(grain + 0.8 * patches - 0.6, 0)
        tint = 1 + 0.12 * clumps - 0.10 * litter + 0.065 * fine
        roughness = 0.93 + 0.027 * clumps - 0.018 * litter
        height = 0.0019 * clumps + 0.0012 * litter
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
