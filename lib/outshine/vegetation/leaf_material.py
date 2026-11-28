"""Coverage-weighted unlit leaf R/T and normal moments for cached near/far sampling."""

from dataclasses import dataclass

import numpy as np

from .leaf_normals import resolve_moments
from .leaf_optics import bounded_optics
from .material_arrays import real_array
from .plan_arrays import immutable_array


def image_size(array):
    if array.ndim < 2 or array.shape[0] != array.shape[1]:
        raise ValueError("square leaf material image required")
    size = len(array)
    if not 1 <= size <= 512 or size & (size - 1):
        raise ValueError("power-of-two leaf material image up to 512 required")
    return size


def conditional_values(optical, moments, coverage):
    values = np.zeros(coverage.shape + (12,))
    np.divide(
        optical, coverage[..., None], out=values[..., :6], where=coverage[..., None] > 0
    )
    np.divide(
        moments, coverage[..., None], out=values[..., 6:], where=coverage[..., None] > 0
    )
    bounded_optics(values[..., :6].reshape(-1, 6))
    return values


@dataclass(frozen=True, eq=False)
class LeafMaterialLevel:
    optical_premultiplied: np.ndarray
    moments_premultiplied: np.ndarray
    coverage: np.ndarray

    def __post_init__(self):
        optical = real_array(self.optical_premultiplied, 6)
        moments = real_array(self.moments_premultiplied, 6)
        coverage = real_array(self.coverage)
        size = image_size(coverage)
        if (
            coverage.shape != (size, size)
            or optical.shape != (size, size, 6)
            or moments.shape != optical.shape
            or np.any((coverage < 0) | (coverage > 1))
            or np.any(optical[coverage == 0] != 0)
            or np.any(moments[coverage == 0] != 0)
        ):
            raise ValueError("matching coverage-weighted leaf images required")
        values = conditional_values(optical, moments, coverage)
        resolve_moments(values[..., 6:])
        for name, value in (
            ("optical_premultiplied", optical),
            ("moments_premultiplied", moments),
            ("coverage", coverage),
        ):
            object.__setattr__(self, name, immutable_array(value))

    def resolve(self):
        values = conditional_values(
            self.optical_premultiplied, self.moments_premultiplied, self.coverage
        )
        normal, roughness, covariance = resolve_moments(values[..., 6:])
        return {
            "reflection": values[..., :3],
            "transmission": values[..., 3:6],
            "normal": normal,
            "roughness": roughness,
            "slope_covariance": covariance,
        }


def leaf_material_mips(optical, coverage, moments):
    optical, coverage, moments = (
        real_array(optical, 6),
        real_array(coverage),
        real_array(moments, 6),
    )
    size = image_size(coverage)
    if optical.shape != (size, size, 6) or moments.shape != optical.shape:
        raise ValueError("matching unlit R/T, coverage and moment images required")
    bounded_optics(optical.reshape(-1, 6))
    resolve_moments(moments)
    if np.any((coverage < 0) | (coverage > 1)):
        raise ValueError("unit leaf coverage required")
    image = np.concatenate(
        (
            optical * coverage[..., None],
            moments * coverage[..., None],
            coverage[..., None],
        ),
        axis=-1,
    )
    levels = []
    while True:
        levels.append(
            LeafMaterialLevel(image[..., :6], image[..., 6:12], image[..., 12])
        )
        if len(image) == 1:
            return tuple(levels)
        image = image.reshape(len(image) // 2, 2, len(image) // 2, 2, 13).mean((1, 3))
