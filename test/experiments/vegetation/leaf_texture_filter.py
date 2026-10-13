"""Integrate procedural vein fields over finite UV texels before caching."""

import numpy as np
from blade_surface import quadrature
from leaf_domain import width_derivative_bound
from leaf_vein_field import segment_field


def physical_points(blade, uv):
    width, _ = blade.width_profile(uv[..., 1])
    return np.stack(((2 * uv[..., 0] - 1) * width, uv[..., 1] * blade.length), axis=-1)


def filtered_veins(blade, a, b, radii, size=512, order=4, texels=None, cull=True):
    if type(size) is not int or not 32 <= size <= 512:
        raise ValueError("32..512 texture size required")
    if type(order) is not int or not 1 <= order <= 16:
        raise ValueError("bounded 1..16 quadrature order required")
    radii = np.asarray(radii, dtype=float)
    if not np.isfinite(radii).all() or np.any(radii <= 0):
        raise ValueError("finite positive vein radii required before support inflation")
    if texels is None:
        texels = np.arange(size * size)
    texels = np.asarray(texels)
    if (
        texels.ndim != 1
        or not np.issubdtype(texels.dtype, np.integer)
        or np.any(texels < 0)
        or np.any(texels >= size * size)
        or len(texels) > size * size
    ):
        raise ValueError("bounded flat texel indices required")
    row, column = np.divmod(texels, size)
    uv = np.column_stack((column + 0.5, row + 0.5)) / size
    result, work = np.zeros(len(texels)), 0
    active = np.arange(len(texels))
    if cull and order > 1:
        bound = np.hypot(blade.width + width_derivative_bound(blade), blade.length)
        inflated = radii + np.nextafter(bound / (6 * size), np.inf)
        support, work = segment_field(
            a, b, inflated, inflated, physical_points(blade, uv)
        )
        active = np.flatnonzero(support > 0)
    nodes, weights = quadrature(order)
    x, y = np.meshgrid(nodes, nodes)
    offsets = np.column_stack((x.ravel(), y.ravel())) / (2 * size)
    weights = np.outer(weights, weights).ravel() / 4
    batch = 262_144 // (order * order)
    for offset in range(0, len(active), batch):
        ids = active[offset : offset + batch]
        points = physical_points(blade, uv[ids, None] + offsets).reshape(-1, 2)
        value, pairs = segment_field(a, b, radii, radii, points)
        result[ids] = np.sum(value.reshape(len(ids), -1) * weights, axis=1)
        work += pairs
    return result, work
