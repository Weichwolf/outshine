"""Exact vertical single scattering for exponential Rayleigh/Mie and tent ozone.

For view and Sun both at the zenith, eye-to-point times point-to-top
transmittance equals the constant eye-to-top transmittance. Radiance therefore
needs only analytic scattering columns. No coefficient fit, quadrature or LUT
is used by this reference; Psi, clouds and ground reflection are excluded.
"""

from dataclasses import dataclass
from pathlib import Path

import numpy as np


@dataclass(frozen=True)
class VerticalAir:
    values: np.ndarray

    @classmethod
    def read(cls, path: Path):
        values = np.fromfile(path, dtype="<f4").astype(np.float64)
        if values.shape != (20,) or not np.isfinite(values).all():
            raise ValueError("expected the native Medium80 finite float layout")
        if (
            not 0 < values[0] < values[1]
            or np.any(values[2:4] <= 0)
            or np.any(values[4:12] < 0)
            or values[13] <= 0
            or abs(values[14]) >= 1
        ):
            raise ValueError("invalid exponential-air parameters")
        values.flags.writeable = False
        return cls(values)

    @property
    def top_height_km(self):
        return self.values[1] - self.values[0]

    def ozone_area(self, height):
        centre, width = self.values[12:14]
        low = np.clip(np.asarray(height) - (centre - width), 0, width)
        high = np.clip((centre + width) - np.asarray(height), 0, width)
        return np.where(
            np.asarray(height) <= centre,
            low * low / (2 * width),
            width - high * high / (2 * width),
        )

    def columns(self, height, distance):
        scales = self.values[2:4]
        exponential = (
            scales
            * np.exp(-np.asarray(height)[..., None] / scales)
            * -np.expm1(-np.asarray(distance)[..., None] / scales)
        )
        ozone = self.ozone_area(height + distance) - self.ozone_area(height)
        return exponential, ozone

    def optical_depth(self, height, distance):
        columns, ozone = self.columns(height, distance)
        return (
            columns[..., 0, None] * self.values[4:7]
            + columns[..., 1, None] * self.values[11]
            + ozone[..., None] * self.values[8:11]
        )

    def phase_weighted_scattering(self, rayleigh, mie):
        g = self.values[14]
        rayleigh_phase = 3 / (8 * np.pi)
        mie_phase = 3 / (4 * np.pi) * (1 + g) / ((2 + g * g) * (1 - g) ** 2)
        return (
            np.asarray(rayleigh)[..., None] * self.values[4:7] * rayleigh_phase
            + np.asarray(mie)[..., None] * self.values[7] * mie_phase
        )

    def transfer(self, height, distance):
        columns, _ = self.columns(height, distance)
        to_top = np.exp(self.optical_depth(height, self.top_height_km - height) * -1)
        radiance = to_top * self.phase_weighted_scattering(
            columns[..., 0], columns[..., 1]
        )
        return radiance, np.exp(-self.optical_depth(height, distance))

    def coefficients(self, height):
        densities = np.exp(-np.asarray(height)[..., None] / self.values[2:4])
        tent = np.clip(
            1 - np.abs(np.asarray(height) - self.values[12]) / self.values[13], 0, 1
        )
        extinction = (
            densities[..., 0, None] * self.values[4:7]
            + densities[..., 1, None] * self.values[11]
            + tent[..., None] * self.values[8:11]
        )
        direct = np.exp(-self.optical_depth(height, self.top_height_km - height))
        emission = direct * self.phase_weighted_scattering(
            densities[..., 0], densities[..., 1]
        )
        return extinction, emission
