"""Same compact furrow function, dense control versus bounded periodic pixel spans."""

import numpy as np
from bark_furrows import compact_cut, longitudinal, periodic_delta


def checked_shape(shape):
    if (
        len(shape) != 2
        or any(type(n) is not int or not 32 <= n <= 4096 for n in shape)
        or shape[0] * shape[1] > 8_388_608
    ):
        raise ValueError("explicit 32..4096 raster axes and at most8M pixels required")


def primary(recipe, plan, shape, method):
    h, w = shape
    dx, dz = recipe.width_m / w, recipe.height_m / h
    z = (np.arange(h) + 0.5) * dz
    rows = np.arange(h)[:, None]
    field = np.zeros(shape)
    evaluations = 0
    for index in range(recipe.furrows):
        centre, width, depth = longitudinal(recipe, plan, index, z)
        if method == "dense":
            columns = np.broadcast_to(np.arange(w), shape)
        else:
            span = int(np.ceil(3 * width.max() / dx)) + 1
            columns = (
                np.floor(centre[:, None] / dx).astype(int) + np.arange(-span, span + 1)
            ) % w
        x = (columns + 0.5) * dx
        offset = periodic_delta(x, centre[:, None], recipe.width_m) / width[:, None]
        value = depth[:, None] * (
            0.65 * compact_cut(offset) + 0.35 * compact_cut(offset / 3)
        )
        field[rows, columns] = np.maximum(field[rows, columns], value)
        evaluations += value.size
    return field, evaluations


def transverse(recipe, plan, field, method):
    h, w = field.shape
    dx, dz = recipe.width_m / w, recipe.height_m / h
    evaluations = 0
    for index, (x, z, length, width, angle, bend, depth) in enumerate(
        plan["fractures"]
    ):
        c, s = np.cos(angle), np.sin(angle)
        if method == "dense":
            columns, rows = np.arange(w), np.arange(h)
        else:
            bx = abs(c) * length + abs(s) * (1.9 * width + abs(bend))
            bz = abs(s) * length + abs(c) * (1.9 * width + abs(bend))
            nx, nz = int(np.ceil(bx / dx)) + 1, int(np.ceil(bz / dz)) + 1
            columns = (int(np.floor(x / dx)) + np.arange(-nx, nx + 1)) % w
            rows = (int(np.floor(z / dz)) + np.arange(-nz, nz + 1)) % h
        u = periodic_delta((columns[None] + 0.5) * dx, x, recipe.width_m)
        v = periodic_delta((rows[:, None] + 0.5) * dz, z, recipe.height_m)
        along = (u * c + v * s) / length
        jagged = (
            0.9
            * width
            * np.polynomial.chebyshev.chebval(
                np.clip(along, -1, 1), plan["fracture_coefficients"][index]
            )
        )
        across = (-u * s + v * c - bend * (1 - along**2) - jagged) / width
        value = depth * compact_cut(along) * compact_cut(across)
        target = np.ix_(rows, columns)
        field[target] = np.maximum(field[target], value)
        evaluations += value.size
    return evaluations


def raster(recipe, shape=(2048, 1024), method="indexed"):
    checked_shape(shape)
    if method not in ("dense", "indexed"):
        raise ValueError("declared raster method required")
    plan = recipe.plan()
    field, primary_evaluations = primary(recipe, plan, shape, method)
    transverse_evaluations = transverse(recipe, plan, field, method)
    return field, {
        "primary_pixel_evaluations": primary_evaluations,
        "transverse_pixel_evaluations": transverse_evaluations,
        "output_pixels": field.size,
    }
