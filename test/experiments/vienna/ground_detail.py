"""Tileable PBR detail shared by the reference renderers, in metre units."""

from functools import cache

import numpy as np
from leaf_litter import bake_litter
from scipy.ndimage import gaussian_filter, map_coordinates

DETAIL_SPAN_M = 2.56
MACRO_SPAN_M = 23.0
HEIGHT_SPAN_M = 0.024
BARK_BOX_BLEND = 0.35
CANOPY_DETAIL = "regional canopy"
CROWN_DETAIL = "crown sprays"
DETAIL_SCALES = {CANOPY_DETAIL: (512.0, 1523.0, 2.4), CROWN_DETAIL: (24.8, 127.9, 0.48)}
DETAIL_TINTS = {"beech bark": (0.86, 1.0, 0.78), "leaf litter": (0.53, 1.26, 0.66)}
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
    "forest floor": "leaf litter",
    "regional wood": CANOPY_DETAIL,
    "broadleaf crown": CROWN_DETAIL,
    "needle crown": CROWN_DETAIL,
    "grass": "grass",
    "meadow": "meadow",
    "unpaved path": "compacted",
    "wall": "plaster",
    "street base": "plaster",
    "street trim": "concrete",
    "oak bark": "oak bark",
    "beech bark": "beech bark",
    "chestnut bark": "chestnut bark",
    "spruce bark": "spruce bark",
    "foliage": "leaf",
    "woodland low leaves": "leaf",
    "weir overflow": "weir overflow",
}


def noise(rng, scale):
    signal = gaussian_filter(rng.standard_normal((SIZE, SIZE)), scale, mode="wrap")
    return np.clip(signal / signal.std(), -3, 3)


def detail_scale(name):
    return DETAIL_SCALES.get(name, (DETAIL_SPAN_M, MACRO_SPAN_M, HEIGHT_SPAN_M))


def regional_canopy(rng):
    """Bake overlapping crown profiles on a periodic eight-metre grid."""
    cells = 64
    y, x = (np.indices((SIZE, SIZE), dtype=np.float32) + 0.5) * (cells / SIZE)
    row, column = np.floor(y).astype(int), np.floor(x).astype(int)
    offsets = rng.uniform(0.10, 0.90, (2, cells, cells))
    radii = rng.uniform(0.58, 1.03, (cells, cells))
    heights = rng.uniform(0.70, 1.13, (cells, cells))
    crown = np.zeros_like(x)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            j, i = row + dy, column + dx
            key = j % cells, i % cells
            cy, cx = j + offsets[0][key], i + offsets[1][key]
            radius_squared = ((x - cx) ** 2 + (y - cy) ** 2) / radii[key] ** 2
            lobe = heights[key] * np.maximum(1 - radius_squared, 0) ** 0.72
            crown = np.maximum(crown, lobe)
    crown = gaussian_filter(crown, 0.7, mode="wrap")
    tint = 0.69 + 0.40 * crown + 0.042 * noise(rng, 4)
    roughness = 0.91 - 0.07 * crown
    height = 1.7 * (crown - 0.43)
    macro = 0.5 + 0.18 * noise(rng, 20)
    return tint, roughness, height, macro


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


def beech_scars(rng):
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    scars = np.zeros((SIZE, SIZE), dtype=np.float32)
    centres = rng.uniform(0, SIZE, (30, 2))
    dimensions = rng.uniform((4, 1.3), (17, 3.1), centres.shape)
    for centre, (width, rise) in zip(centres, dimensions, strict=True):
        across = ((x - centre[0] + SIZE / 2) % SIZE - SIZE / 2) / width
        along = ((y - centre[1] + SIZE / 2) % SIZE - SIZE / 2) / rise
        lower_rim = np.exp(-((along - 1.2 * across**2) ** 2 + 1.6 * across**2))
        bud = np.exp(-((along + 0.8) ** 2 + 4.8 * across**2))
        scars = np.maximum(scars, 0.72 * lower_rim + 0.44 * bud)
    return scars


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
        "leaf litter",
        "chestnut bark",
        CANOPY_DETAIL,
        "spruce bark",
        CROWN_DETAIL,
    ).index(name)
    rng = np.random.default_rng(seed)
    fine, grain, patches = (noise(rng, scale) for scale in (0.65, 1.6, 12))
    macro = 0.5 + 0.15 * patches
    if name == CANOPY_DETAIL:
        tint, roughness, height, macro = regional_canopy(rng)
    elif name == CROWN_DETAIL:
        sprays = np.tanh(0.54 * noise(rng, 11) + 0.47 * noise(rng, 24))
        shade = np.maximum(sprays, 0)
        tint = 0.83 + 0.25 * shade + 0.055 * grain
        roughness = 0.84 - 0.055 * shade
        height = 0.12 * sprays + 0.035 * noise(rng, 5)
    elif name == "asphalt":
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
        wear = np.tanh(0.8 * patches + 0.55 * noise(rng, (29, 19)))
        exposed = np.clip(0.45 + 0.24 * wear + 0.32 * noise(rng, 6), 0, 1)
        aggregate = exposed * np.clip(grain + 0.32 * fine - 0.75, 0, 1.7)
        tint = 0.90 + 0.14 * wear + 0.17 * aggregate + 0.047 * fine
        roughness = 0.90 - 0.047 * aggregate + 0.023 * wear
        height = 0.0030 * aggregate + 0.00045 * fine + 0.00085 * wear
    elif name == "soil":
        humus = noise(rng, 30)
        litter = np.maximum(noise(rng, (2.5, 0.95)) - 0.7, 0)
        tint = 0.97 + 0.14 * litter + 0.055 * grain + 0.075 * humus
        roughness = 0.92 - 0.065 * litter + 0.025 * patches
        height = 0.0025 * litter + 0.00070 * fine + 0.0021 * humus
    elif name == "leaf litter":
        tint, roughness, height, macro = bake_litter(
            rng, fine, grain, noise(rng, 30), noise(rng, 15), patches, DETAIL_SPAN_M
        )
    elif name == "plaster":
        pits = np.maximum(-fine - 0.9, 0)
        trowel = noise(rng, (9, 19))
        tint = 1 - 0.045 * pits + 0.022 * grain + 0.026 * trowel
        roughness = 0.83 + 0.028 * pits + 0.024 * trowel
        height = -0.00060 * pits + 0.00035 * grain + 0.00060 * trowel
    elif name == "oak bark":
        y, x = np.indices((SIZE, SIZE), dtype=np.float32)
        shift = 5.2 * noise(rng, (25, 13)) + 3.2 * noise(rng, (7, 4))
        grain_flow = map_coordinates(
            noise(rng, (15, 2.2)), [y, x + shift], order=1, mode="grid-wrap"
        )
        plates = noise(rng, (9, 4.5))
        breaks = np.clip(-noise(rng, (1.8, 5)) + 0.48 * plates - 0.82, 0, 1)
        fissures = np.clip((0.32 - abs(grain_flow + 0.30 * plates)) * 3.2, 0, 1)
        raised = np.tanh(0.8 * plates + 0.65 * noise(rng, (28, 5)))
        tint = 1.01 - 0.68 * fissures - 0.32 * breaks + 0.12 * patches + 0.26 * raised
        roughness = 0.90 + 0.032 * fissures + 0.019 * grain
        height = -0.0080 * fissures - 0.0034 * breaks + 0.0030 * raised + 0.00064 * fine
    elif name == "beech bark":
        mottling = 0.65 * noise(rng, (22, 14)) + 0.35 * noise(rng, (6, 5))
        pores = np.maximum(noise(rng, (2.1, 4.3)) + 0.25 * mottling - 1.25, 0)
        lichen = np.clip(
            noise(rng, (14, 10)) + 0.38 * patches + 0.28 * grain - 0.56, 0, 1
        )
        scars = 0.36 * pores + beech_scars(rng)
        tint = 0.81 + 0.22 * mottling + 0.38 * lichen + 0.055 * grain - 0.64 * scars
        roughness = 0.86 + 0.027 * mottling + 0.015 * lichen + 0.021 * patches
        height = -0.0027 * scars + 0.00088 * lichen + 0.00034 * grain
        macro = lichen
    elif name == "chestnut bark":
        plates = noise(rng, (14, 5)) + 0.45 * noise(rng, (25, 9))
        lifted = np.clip(0.5 + 0.65 * plates, 0, 1)
        seams = np.exp(-(((plates + 0.29) / 0.27) ** 2))
        split = np.maximum(0.7 * noise(rng, (5, 2)) - 0.38, 0)
        tint = 0.78 + 0.40 * lifted - 0.33 * seams - 0.16 * split + 0.14 * patches
        roughness = 0.89 + 0.023 * grain + 0.034 * seams
        height = 0.0043 * lifted - 0.0037 * seams - 0.0016 * split + 0.00045 * fine
    elif name == "spruce bark":
        plates = noise(rng, (5.4, 4.0)) + 0.42 * noise(rng, (12, 8))
        lifted = np.clip(0.55 + 0.77 * plates, 0, 1)
        rims = np.exp(-(((plates + 0.13) / 0.30) ** 2))
        tint = 0.74 + 0.42 * lifted - 0.28 * rims + 0.19 * patches + 0.044 * fine
        roughness = 0.91 + 0.021 * grain + 0.030 * rims
        height = 0.0048 * lifted - 0.0028 * rims + 0.00062 * fine
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
            np.clip(0.5 + height / detail_scale(name)[2], 0, 1),
            np.clip(macro, 0, 1),
        ),
        axis=-1,
    ).astype("f4")
    if name in ("leaf", "beech bark", "chestnut bark", CROWN_DETAIL):
        half = SIZE // 2
        return pixels.reshape(half, 2, half, 2, 4).mean(axis=(1, 3))
    return pixels
