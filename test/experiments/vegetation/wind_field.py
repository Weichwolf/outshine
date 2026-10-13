"""Bounded continuous wind displacement with reusable values and exact gradients."""

from dataclasses import dataclass

import numpy as np


def smoothstep(value):
    x = np.clip(value, 0, 1)
    return x * x * (3 - 2 * x), 6 * x * (1 - x)


@dataclass(frozen=True)
class WindField:
    origins: np.ndarray
    tangents: np.ndarray
    lengths: np.ndarray
    supports: np.ndarray
    amplitudes: np.ndarray
    frequencies: np.ndarray
    phases: np.ndarray
    direction: np.ndarray
    height: float

    def __post_init__(self):
        object.__setattr__(self, "height", float(self.height))
        count = len(self.lengths)
        if not 1 <= count <= 32 or not np.isfinite(self.height) or self.height <= 0:
            raise ValueError("positive height and 1..32 wind modes required")
        for name in self.__dataclass_fields__:
            if name == "height":
                continue
            value = np.array(getattr(self, name), dtype=float, copy=True)
            shape = (3,) if name == "direction" else (count,)
            if name in ("origins", "tangents"):
                shape = (count, 3)
            if value.shape != shape or not np.isfinite(value).all():
                raise ValueError("finite complete wind arrays required")
            value.flags.writeable = False
            object.__setattr__(self, name, value)
        if (
            np.any(self.lengths <= 0)
            or np.any(self.supports <= 0)
            or np.any(self.frequencies <= 0)
            or not np.allclose(
                np.linalg.norm(self.tangents, axis=1), 1, atol=1e-12, rtol=0
            )
            or abs(np.linalg.norm(self.direction) - 1) > 1e-12
            or self.direction[2] != 0
            or not np.array_equal(self.origins[0], np.zeros(3))
        ):
            raise ValueError(
                "unit axes, horizontal wind and rooted positive modes required"
            )
        if self.determinant_lower_bound <= 0.1:
            raise ValueError("wind amplitudes exceed the deformation budget")

    @property
    def determinant_lower_bound(self):
        radial_bound = 6 / np.sqrt(5) * (4 / 5) ** 2
        return float(
            1
            - np.sum(
                np.abs(self.amplitudes[1:])
                * (1.5 / self.lengths[1:] + radial_bound / self.supports[1:])
            )
        )

    def coefficients(self, seconds, strength=1.0):
        if (
            not np.isfinite(seconds)
            or not np.isfinite(strength)
            or not 0 <= strength <= 1
        ):
            raise ValueError("finite time and strength in [0,1] required")
        return (
            strength
            * self.amplitudes
            * np.sin(2 * np.pi * self.frequencies * seconds + self.phases)
        )

    def bind(self, points):
        points = np.asarray(points, dtype=float)
        if points.ndim != 2 or points.shape[1] != 3:
            raise ValueError("Nx3 rest points required")
        if len(points) * len(self.lengths) > 1_000_000:
            raise ValueError("wind binding exceeds one million point-mode pairs")
        if not np.isfinite(points).all():
            raise ValueError("finite rest points required")
        values = np.zeros((len(points), len(self.lengths)))
        gradients = np.zeros((*values.shape, 3))
        height = np.maximum(points[:, 2] / self.height, 0)
        values[:, 0] = np.where(height <= 1, height**2, 2 * height - 1)
        gradients[:, 0, 2] = 2 * np.minimum(height, 1) / self.height
        gate, gate_d = smoothstep(height)
        for i in range(1, len(self.lengths)):
            relative = points - self.origins[i]
            along = relative @ self.tangents[i]
            radial = relative - along[:, None] * self.tangents[i]
            q = np.maximum(1 - np.sum(radial**2, axis=1) / self.supports[i] ** 2, 0)
            axial, axial_d = smoothstep(along / self.lengths[i])
            raw = axial * q**3
            values[:, i] = raw * gate
            gradients[:, i] = gate[:, None] * (
                (axial_d * q**3 / self.lengths[i])[:, None] * self.tangents[i]
                - (6 * axial * q**2 / self.supports[i] ** 2)[:, None] * radial
            )
            gradients[:, i, 2] += raw * gate_d / self.height
        return values, gradients

    def move(self, points, binding, seconds, strength=1.0):
        values, gradients = binding
        coefficients = self.coefficients(seconds, strength)
        displacement = values @ coefficients
        gradient = np.einsum("nmc,m->nc", gradients, coefficients)
        return points + displacement[:, None] * self.direction, gradient

    def normals(self, normals, gradient):
        determinant = 1 + gradient @ self.direction
        result = (
            normals - gradient * ((normals @ self.direction) / determinant)[:, None]
        )
        return result / np.linalg.norm(result, axis=1)[:, None]

    def arrays(self):
        return {name: getattr(self, name) for name in self.__dataclass_fields__}
