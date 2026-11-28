"""Physical tangent slopes and linearly filterable surface moments."""

import numpy as np

from .material_arrays import real_array


def tangent_slopes(du, dv, height_u, height_v):
    du, dv = real_array(du, 3), real_array(dv, 3)
    height_u, height_v = real_array(height_u), real_array(height_v)
    if height_u.shape != du.shape[:-1] or height_v.shape != du.shape[:-1]:
        raise ValueError("matching physical height derivative shape required")
    if du.shape != dv.shape or du.shape[-1:] != (3,) or not np.isfinite([du, dv]).all():
        raise ValueError("matching finite 3D surface derivatives required")
    length = np.linalg.norm(du, axis=-1)
    normal = np.cross(du, dv)
    area = np.linalg.norm(normal, axis=-1)
    if np.any(length <= 0) or np.any(area <= 0):
        raise ValueError("regular surface metric required")
    tangent, normal = du / length[..., None], normal / area[..., None]
    bitangent = np.cross(normal, tangent)
    vertical = np.sum(bitangent * dv, axis=-1)
    x = -np.asarray(height_u) / length
    y = (-np.asarray(height_v) - x * np.sum(tangent * dv, axis=-1)) / vertical
    result = np.stack((x, y), axis=-1)
    if not np.isfinite(result).all():
        raise ValueError("finite physical height derivatives required")
    return result


def normal_slopes(blade, height):
    height = real_array(height)
    size = len(height)
    if height.shape != (size, size) or not 4 <= size <= 512:
        raise ValueError("bounded square physical height map required")
    if not np.isfinite(height).all():
        raise ValueError("finite metre heights required")
    u, v = np.meshgrid((np.arange(size) + 0.5) / size, (np.arange(size) + 0.5) / size)
    _, _, _, du, dv = blade.surface_jet(v, 2 * u - 1, np.where(u < 0.5, -1, 1))
    height_v, height_u = np.gradient(height, 1 / size, edge_order=2)
    return tangent_slopes(du, dv, height_u, height_v)


def slope_moments(slopes, roughness):
    slopes, roughness = real_array(slopes, 2), real_array(roughness)
    size = len(slopes)
    if (
        slopes.shape != (size, size, 2)
        or roughness.shape != (size, size)
        or size < 1
        or size > 512
        or size & (size - 1)
        or not np.isfinite(slopes).all()
        or not np.isfinite(roughness).all()
        or np.any((roughness < 0) | (roughness > 1))
    ):
        raise ValueError("bounded power-of-two slopes and unit roughness required")
    x, y = slopes[..., 0], slopes[..., 1]
    return np.stack((x, y, x * x, x * y, y * y, roughness**4), axis=-1)


def mip_pyramid(base):
    base = real_array(base, 6)
    size = len(base)
    if (
        base.shape != (size, size, 6)
        or size < 1
        or size > 512
        or size & (size - 1)
        or not np.isfinite(base).all()
    ):
        raise ValueError("bounded power-of-two six-channel moments required")
    levels = [base.copy()]
    while len(levels[-1]) > 1:
        image = levels[-1]
        levels.append(
            image.reshape(len(image) // 2, 2, len(image) // 2, 2, 6).mean((1, 3))
        )
    return levels


def resolve_moments(moments, broaden=True):
    moments = real_array(moments, 6)
    if type(broaden) is not bool:
        raise TypeError("explicit normal variance filtering flag required")
    mean = moments[..., :2]
    variance_x = moments[..., 2] - mean[..., 0] ** 2
    variance_y = moments[..., 4] - mean[..., 1] ** 2
    cross = moments[..., 3] - mean[..., 0] * mean[..., 1]
    covariance = np.stack((variance_x, cross, variance_y), axis=-1)
    tolerance = (
        8
        * np.finfo(np.float32).eps
        * np.maximum(np.max(abs(moments[..., 2:5]), axis=-1), 1)
    )
    if (
        np.any(variance_x < -tolerance)
        or np.any(variance_y < -tolerance)
        or np.any(cross * cross > (variance_x + tolerance) * (variance_y + tolerance))
        or np.any((moments[..., 5] < 0) | (moments[..., 5] > 1))
    ):
        raise ValueError("valid slope covariance and fourth-power roughness required")
    normal = np.concatenate((mean, np.ones(mean.shape[:-1] + (1,))), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1)[..., None]
    alpha_squared = moments[..., 5]
    if broaden:
        alpha_squared = alpha_squared + np.maximum(variance_x + variance_y, 0)
    roughness = np.minimum(np.maximum(alpha_squared, 0) ** 0.25, 1)
    return normal, roughness, covariance
