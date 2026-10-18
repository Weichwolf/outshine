"""Own cached metallic/roughness bark maps with shared physical height derivatives."""

import numpy as np
from scipy.ndimage import map_coordinates


def smooth_noise(seed, grid, shape):
    rng = np.random.default_rng(seed)
    samples = rng.uniform(-1, 1, grid)
    y, x = np.meshgrid(
        (np.arange(shape[0]) + 0.5) * grid[0] / shape[0],
        (np.arange(shape[1]) + 0.5) * grid[1] / shape[1],
        indexing="ij",
    )
    return np.clip(map_coordinates(samples, (y, x), order=3, mode="grid-wrap"), -1, 1)


def metric_normals(height, width_m, height_m):
    dz = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) / (
        2 * height_m / height.shape[0]
    )
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) / (
        2 * width_m / height.shape[1]
    )
    normal = np.stack((-dx, -dz, np.ones_like(height)), axis=2)
    normal /= np.linalg.norm(normal, axis=2)[:, :, None]
    return normal


def maps(recipe, relief):
    # Each band has a separate stable stream and fixed source coefficient grid.
    macro = smooth_noise(
        np.random.SeedSequence((recipe.seed, 1)), (8, 16), relief.shape
    )
    grain = smooth_noise(
        np.random.SeedSequence((recipe.seed, 2)), (64, 128), relief.shape
    )
    micro = smooth_noise(
        np.random.SeedSequence((recipe.seed, 3)), (256, 256), relief.shape
    )
    depth = relief / 0.007
    crust = 0.001 * (1 - abs(grain)) + 0.0005 * (1 - abs(micro))
    height = 0.010 - relief + crust * (1 - 0.8 * depth)
    pigment = (1 - 0.6 * depth) * (1 + 0.22 * macro + 0.25 * grain)
    colour = pigment[:, :, None] * np.array((0.12, 0.105, 0.085))
    colour += micro[:, :, None] * 0.012
    roughness = np.clip(0.82 + 0.12 * depth - 0.04 * grain, 0, 1)
    return {
        "height_m": height.astype(np.float32),
        "colour_linear": colour.astype(np.float32),
        "roughness": roughness.astype(np.float32),
        "normal": metric_normals(height, recipe.width_m, recipe.height_m).astype(
            np.float32
        ),
    }
