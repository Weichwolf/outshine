"""Bake the meadow's lower canopy into a repeatable terrain material."""

import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import gaussian_filter

from outshine.vegetation.meadow import MEADOW_CANDIDATES_M2, meadow_tuft
from outshine.vegetation.plant_sites import scatter_mesh

MEADOW_TILE_M = 5.12


def canopy_tile(rng, size):
    cells = int(MEADOW_TILE_M * np.sqrt(MEADOW_CANDIDATES_M2))
    grid = np.indices((cells, cells)).reshape(2, -1).T
    sites = (grid + rng.random(grid.shape)) * (MEADOW_TILE_M / cells)
    anchors = np.column_stack((sites, np.zeros(len(sites))))
    tall = rng.random(len(sites)) < 0.28
    angle = rng.uniform(0, 2 * np.pi, len(sites))
    scale = rng.uniform(0.85, 1.55, len(sites))
    surface = []
    for stature in (False, True):
        group = tall == stature
        _, canopy, _ = meadow_tuft(stature)
        vertices, faces = scatter_mesh(
            canopy, anchors[group], angle[group], scale[group]
        )
        surface.append(vertices[faces])
    triangles = np.concatenate(surface)
    height = triangles[:, :, 2].mean(axis=1)
    texels = triangles[:, :, :2] * (size / MEADOW_TILE_M)
    wrapped, depths = [], []
    lower, upper = texels.min(axis=1), texels.max(axis=1)
    for east in (-size, 0, size):
        for north in (-size, 0, size):
            shift = np.array((east, north))
            visible = ((upper + shift >= 0) & (lower + shift < size)).all(axis=1)
            wrapped.append(texels[visible] + shift)
            depths.append(height[visible])
    texels, height = np.concatenate(wrapped), np.concatenate(depths)
    canvas = Image.new("L", (size, size))
    paint = ImageDraw.Draw(canvas)
    order = np.argsort(height, kind="stable")
    shade = np.clip(1 + height * 253 / 0.65, 1, 254).astype(np.uint8)
    for i in order:
        paint.polygon(tuple(map(tuple, texels[i])), fill=int(shade[i]))
    pixels = np.asarray(canvas, dtype=np.float32)
    return gaussian_filter(
        pixels > 0, 0.65, mode="wrap", output=np.float32
    ), pixels / 254


def bake_meadow(rng, size, fine, patches):
    coverage, height = canopy_tile(rng, size)
    shelter = gaussian_filter(coverage, 5, mode="wrap")
    tint = 0.73 + 0.27 * coverage - 0.15 * shelter + 0.035 * fine
    roughness = 0.93 - 0.065 * coverage + 0.015 * patches
    relief = 0.0050 * gaussian_filter(height, 0.8, mode="wrap") + 0.00028 * fine
    macro = np.clip(0.5 + 0.17 * patches, 0, 1)
    return tint, roughness, relief, macro
