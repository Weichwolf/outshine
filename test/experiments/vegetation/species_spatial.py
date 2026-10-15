"""Experimental world-anchored Gaussian copula for correlated species marks.

Finite Fourier covariance is approximate; pointwise normal marginals are exact
in the ideal Gaussian coefficient ensemble. This is not an ecosystem simulator.
"""

from dataclasses import dataclass

import numpy as np
from outshine_ref.vegetation.plan_arrays import immutable_array
from scipy.special import ndtr, ndtri


@dataclass(frozen=True, eq=False)
class FourierMarks:
    waves: np.ndarray
    coefficients: np.ndarray

    def __post_init__(self):
        w, c = np.asarray(self.waves), np.asarray(self.coefficients)
        if (
            w.ndim != 2
            or w.shape[1:] != (3,)
            or not 1 <= len(w) <= 1024
            or c.shape != (len(w), 2)
            or w.dtype.kind not in "fiu"
            or c.dtype.kind not in "fiu"
            or not np.isfinite(w).all()
            or not np.isfinite(c).all()
            or np.any(abs(w) > 1)
            or np.any(abs(c) > 100)
        ):
            raise ValueError(
                "bounded finite physical wavevectors and Gaussian pairs required"
            )
        object.__setattr__(self, "waves", immutable_array(w.astype(np.float64)))
        object.__setattr__(self, "coefficients", immutable_array(c.astype(np.float64)))

    @classmethod
    def prepare(cls, seed, length_m=60.0, count=256):
        if (
            type(seed) is not int
            or not 0 <= seed < 2**64
            or not np.isfinite(length_m)
            or not 10 <= length_m <= 10000
            or type(count) is not int
            or not 1 <= count <= 1024
        ):
            raise ValueError(
                "uint64 seed, 10..10000m length and 1..1024 waves required"
            )
        rng = np.random.Generator(np.random.PCG64(seed))
        return cls(rng.normal(size=(count, 3)) / length_m, rng.normal(size=(count, 2)))

    def recipe(self):
        return {
            "format": "fourier-species-marks-1",
            "coordinate_frame": "EPSG:4978-metres",
            "waves_rad_per_m": self.waves.tolist(),
            "coefficients": self.coefficients.tolist(),
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            recipe["format"] != "fourier-species-marks-1"
            or recipe["coordinate_frame"] != "EPSG:4978-metres"
        ):
            raise ValueError("explicit prepared Fourier/ECEF mark recipe required")
        return cls(recipe["waves_rad_per_m"], recipe["coefficients"])

    def gaussian(self, world_m, method="stream"):
        p = np.asarray(world_m)
        if (
            p.ndim != 2
            or p.shape[1:] != (3,)
            or len(p) > 1000000
            or p.dtype.kind not in "fiu"
            or not np.isfinite(p).all()
            or np.any(abs(p.astype(np.float64)) > 10000000)
            or method not in ("stream", "matrix")
        ):
            raise ValueError(
                "bounded real ECEF metre points and explicit method required"
            )
        p = p.astype(np.float64, copy=False)
        phase = None
        if method == "matrix":
            if len(p) * len(self.waves) > 16777216:
                raise ValueError("reference phase matrix exceeds explicit budget")
            phase = (
                p[:, 0, None] * self.waves[None, :, 0]
                + p[:, 1, None] * self.waves[None, :, 1]
                + p[:, 2, None] * self.waves[None, :, 2]
            )
        result = np.zeros(len(p))
        for i, (w, (a, b)) in enumerate(
            zip(self.waves, self.coefficients, strict=True)
        ):
            theta = (
                p[:, 0] * w[0] + p[:, 1] * w[1] + p[:, 2] * w[2]
                if phase is None
                else phase[:, i]
            )
            result += a * np.cos(theta) + b * np.sin(theta)
        return result / np.sqrt(len(self.waves))

    def uniforms(self, world_m, site_uniforms, strength=0.85, method="stream"):
        p, u = np.asarray(world_m), np.asarray(site_uniforms)
        if (
            u.shape != (len(p),)
            or u.dtype.kind not in "fiu"
            or not np.isfinite(u).all()
            or np.any((u < 0) | (u >= 1))
            or not np.isfinite(strength)
            or not 0 <= strength <= 1
        ):
            raise ValueError(
                "matching [0,1) site variates and unit correlation strength required"
            )
        field = self.gaussian(p, method)
        if strength == 0:
            return u.copy()
        # Upper-53-bit site hashes can produce zero; bound that single endpoint.
        local = ndtri(np.maximum(u, 2**-54))
        value = np.sqrt(strength) * field + np.sqrt(1 - strength) * local
        return np.minimum(ndtr(value), np.nextafter(1.0, 0.0))
