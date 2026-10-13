"""Procedural leaf material products tied to one physical blade and vein plan."""

import numpy as np
from leaf_vein_field import vein_field


def material_maps(blade, plan, size=512, method="indexed"):
    if type(size) is not int or not 32 <= size <= 512:
        raise ValueError("32..512 leaf texture size required")
    s, t = np.meshgrid(
        2 * (np.arange(size) + 0.5) / size - 1, (np.arange(size) + 0.5) / size
    )
    width, _ = blade.width_profile(t)
    x, y = s * width, t * blade.length
    ridge, work = vein_field(plan, np.column_stack((x.ravel(), y.ravel())), method)
    ridge = ridge.reshape(size, size)
    variation = 0.5 + 0.5 * np.sin(x * 641 + np.sin(y * 239)) * np.sin(y * 437)
    edge = np.abs(s) ** 8
    base = np.array([0.055, 0.17, 0.018])
    colour = base * (0.86 + 0.23 * variation[..., None] - 0.12 * edge[..., None])
    colour = (
        colour * (1 - 0.20 * ridge[..., None])
        + np.array([0.13, 0.24, 0.035]) * 0.20 * ridge[..., None]
    )
    roughness = 0.56 + 0.08 * edge - 0.02 * ridge
    return {"colour": colour, "height": ridge, "roughness": roughness}, work
