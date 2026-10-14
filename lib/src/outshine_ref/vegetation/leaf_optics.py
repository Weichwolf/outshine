"""Prepared hemispherical leaf R/T, linear gamut reduction and shader mixtures."""

from dataclasses import dataclass

import numpy as np

from .material_arrays import real_array, rows
from .plan_arrays import immutable_array

PIGMENT_LIMITS = (80.0, 10.0)


def gamut_map(rgb):
    rgb = np.asarray(rgb)
    if rgb.ndim == 3 and rgb.shape[1:] == (2, 3):
        rgb = rgb.reshape(-1, 6)
    rgb = rows(rgb, 6).reshape(-1, 2, 3)
    luminance = rgb @ np.array([0.2126, 0.7152, 0.0722])
    if np.any(luminance < 0) or np.any(luminance.sum(axis=1) > 1):
        raise ValueError("energy-bounded optical luminance required")
    minimum, maximum = rgb.min(axis=2), rgb.max(axis=2)
    scales = np.ones_like(luminance)
    np.divide(luminance, luminance - minimum, out=scales, where=minimum < 0)
    total_y, total_max = luminance.sum(axis=1), maximum.sum(axis=1)
    energy_scale = np.ones(len(rgb))
    np.divide(1 - total_y, total_max - total_y, out=energy_scale, where=total_max > 1)
    scale = np.minimum(scales.min(axis=1), energy_scale)
    result = luminance[..., None] + scale[:, None, None] * (rgb - luminance[..., None])
    result = np.clip(result, 0, 1)
    return result.reshape(-1, 6), scale


def bounded_optics(value):
    optical = rows(value, 6)
    if (
        np.any(optical < 0)
        or np.any(optical > 1)
        or np.any(optical[:, :3].max(axis=1) + optical[:, 3:].max(axis=1) > 1 + 1e-12)
    ):
        raise ValueError("nonnegative energy-bounded linear leaf R/T required")
    return optical


def optical_mixture(optical):
    optical = bounded_optics(optical)
    reflection, transmission = optical[:, :3], optical[:, 3:]
    rmax, tmax = reflection.max(axis=1), transmission.max(axis=1)
    weight = np.zeros(len(optical))
    np.divide(tmax, rmax + tmax, out=weight, where=rmax + tmax > 0)
    colour, transmitted = np.zeros_like(reflection), np.zeros_like(transmission)
    np.divide(reflection, 1 - weight[:, None], out=colour, where=weight[:, None] < 1)
    np.divide(transmission, weight[:, None], out=transmitted, where=weight[:, None] > 0)
    return colour, transmitted, weight


def lookup(values, pigments, limits=PIGMENT_LIMITS):
    values = real_array(values, 6)
    if (
        values.ndim != 3
        or values.shape[0] != values.shape[1]
        or not 3 <= len(values) <= 65
    ):
        raise ValueError("bounded square six-channel pigment table required")
    limits = real_array(limits)
    if limits.shape != (2,) or np.any(limits <= 0):
        raise ValueError("two positive pigment limits in micrograms/cm² required")
    pigments = rows(pigments, 2)
    if np.any(pigments < 0) or np.any(pigments > limits):
        raise ValueError("pigment coordinates outside table domain")
    size = len(values)
    scaled = np.log1p(pigments) / np.log1p(limits) * (size - 1)
    low = np.minimum(np.floor(scaled).astype(int), size - 2)
    fraction = scaled - low
    x, y = low.T
    u, v = fraction.T
    return (
        ((1 - u) * (1 - v))[:, None] * values[x, y]
        + (u * (1 - v))[:, None] * values[x + 1, y]
        + ((1 - u) * v)[:, None] * values[x, y + 1]
        + (u * v)[:, None] * values[x + 1, y + 1]
    )


@dataclass(frozen=True, eq=False)
class LeafOpticsTable:
    values: np.ndarray
    limits: tuple[float, float] = PIGMENT_LIMITS

    def __post_init__(self):
        values = real_array(self.values, 6)
        lookup(values, np.empty((0, 2)), self.limits)
        gamut_map(values.reshape(-1, 6))
        object.__setattr__(self, "values", immutable_array(values))
        object.__setattr__(self, "limits", tuple(float(v) for v in self.limits))

    def sample(self, pigments):
        return gamut_map(lookup(self.values, pigments, self.limits))[0]

    def recipe(self):
        return {
            "version": "leaf-optics-table-1",
            "colour_space": "linear-srgb",
            "channels": ["R.r", "R.g", "R.b", "T.r", "T.g", "T.b"],
            "coordinates": "log1p-chlorophyll-anthocyanin-ug-cm2",
            "limits": list(self.limits),
            "values": self.values.tolist(),
        }

    @classmethod
    def from_recipe(cls, recipe):
        required = {
            "version",
            "colour_space",
            "channels",
            "coordinates",
            "limits",
            "values",
        }
        if not isinstance(recipe, dict) or set(recipe) != required:
            raise ValueError("complete leaf optics table recipe required")
        if (
            recipe["version"] != "leaf-optics-table-1"
            or recipe["colour_space"] != "linear-srgb"
            or recipe["channels"] != ["R.r", "R.g", "R.b", "T.r", "T.g", "T.b"]
            or recipe["coordinates"] != "log1p-chlorophyll-anthocyanin-ug-cm2"
        ):
            raise ValueError("supported linear R/T table schema required")
        return cls(np.asarray(recipe["values"]), tuple(recipe["limits"]))
