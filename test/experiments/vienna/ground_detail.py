"""Tileable PBR detail shared by the reference renderers, in metre units."""

from functools import cache

import numpy as np
from scipy.ndimage import gaussian_filter

DETAIL_SPAN_M = 2.56
MACRO_SPAN_M = 23.0
HEIGHT_SPAN_M = 0.024
SIZE = 512


SURFACE_DETAIL = {
    "road": "asphalt",
    "paving": "stone",
    "bridge deck": "concrete",
    "stair limestone": "concrete",
    "ground": "soil",
}


def noise(rng, scale):
    signal = gaussian_filter(rng.standard_normal((SIZE, SIZE)), scale, mode="wrap")
    return np.clip(signal / signal.std(), -3, 3)


def stone_setts(rng, grain):
    count = 16
    y, x = np.indices((SIZE, SIZE), dtype=np.float32) * (count / SIZE)
    y += 0.028 * noise(rng, 7)
    x += 0.5 * (np.floor(y).astype(int) & 1) + 0.028 * noise(rng, 7)
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    edge = np.minimum.reduce([x - ix, y - iy, 1 - x + ix, 1 - y + iy])
    face = np.clip((edge - 0.025) / 0.05, 0, 1)
    tint = rng.uniform(-1, 1, (count, count))[iy % count, ix % count]
    return (
        0.72 + 0.26 * face + 0.12 * tint + 0.03 * grain,
        0.88 - 0.15 * face + 0.025 * grain,
        0.0035 * face + 0.0004 * grain,
    )


@cache
def ground_detail(name):
    seed = ("asphalt", "stone", "concrete", "soil").index(name)
    rng = np.random.default_rng(seed)
    fine, grain, patches = (noise(rng, scale) for scale in (0.65, 1.6, 12))
    if name == "asphalt":
        tint = 1 + 0.095 * fine + 0.055 * grain
        roughness = 0.84 + 0.035 * grain + 0.02 * patches
        height = 0.00055 * fine + 0.0004 * grain
    elif name == "stone":
        tint, roughness, height = stone_setts(rng, fine)
    elif name == "concrete":
        pits = np.maximum(-fine - 1.35, 0)
        tint = 1 - 0.10 * pits + 0.035 * grain
        roughness = 0.75 + 0.04 * pits + 0.035 * patches
        height = -0.0007 * pits + 0.00035 * grain
    else:
        tint = 1 + 0.045 * fine + 0.065 * grain + 0.04 * patches
        roughness = 0.85 + 0.04 * grain + 0.025 * patches
        height = 0.00055 * fine + 0.0015 * grain + 0.0018 * patches
    return np.stack(
        (
            np.clip(tint * 0.5, 0, 1),
            np.clip(roughness, 0.05, 1),
            np.clip(0.5 + height / HEIGHT_SPAN_M, 0, 1),
            np.clip(0.5 + 0.15 * patches, 0, 1),
        ),
        axis=-1,
    ).astype("f4")
