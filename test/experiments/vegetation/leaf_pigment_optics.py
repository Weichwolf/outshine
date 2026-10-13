"""Bounded pigment interpolation over an external PROSPECT-D reference model."""

from functools import lru_cache

import colour
import numpy as np
import prosail

FIXED = {"n": 1.3, "car": 10.0, "cbrown": 0.0, "cw": 0.012, "cm": 0.010, "alpha": 40.0}
LIMITS = np.array([80.0, 10.0])


@lru_cache(maxsize=1)
def rgb_weights():
    shape = colour.SpectralShape(400, 780, 1)
    observer = (
        colour.MSDS_CMFS["CIE 1931 2 Degree Standard Observer"].copy().align(shape)
    )
    light = colour.SDS_ILLUMINANTS["D65"].copy().align(shape)
    xyz = observer.values * light.values[:, None]
    matrix = colour.RGB_COLOURSPACES["sRGB"].matrix_XYZ_to_RGB
    weights = xyz @ matrix.T
    weights /= weights.sum(axis=0)
    weights.setflags(write=False)
    return weights


def direct_colour(chlorophyll, anthocyanin):
    pigments = np.array([chlorophyll, anthocyanin])
    if (
        not np.isfinite(pigments).all()
        or np.any(pigments < 0)
        or np.any(pigments > LIMITS)
    ):
        raise ValueError("bounded pigment contents in micrograms/cm² required")
    wavelength, reflection, transmission = prosail.run_prospect(
        cab=float(chlorophyll), ant=float(anthocyanin), prospect_version="D", **FIXED
    )
    assert np.array_equal(wavelength, np.arange(400, 2501))
    assert np.isfinite([reflection, transmission]).all()
    assert min(reflection.min(), transmission.min()) >= 0
    assert np.max(reflection + transmission) <= 1 + 1e-12
    rgb = np.concatenate(
        (reflection[:381] @ rgb_weights(), transmission[:381] @ rgb_weights())
    )
    return rgb


def gamut_map(rgb):
    rgb = np.asarray(rgb, float).reshape(-1, 2, 3)
    luminance = rgb @ np.array([0.2126, 0.7152, 0.0722])
    if (
        not np.isfinite(rgb).all()
        or np.any(luminance < 0)
        or np.any(luminance.sum(axis=1) > 1)
    ):
        raise ValueError("finite energy-bounded optical luminance required")
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


def build_table(size):
    if type(size) is not int or not 3 <= size <= 65:
        raise ValueError("3..65 samples per pigment dimension required")
    axes = [np.expm1(np.linspace(0, np.log1p(limit), size)) for limit in LIMITS]
    for axis, limit in zip(axes, LIMITS):
        axis[-1] = limit
    values = np.array([[direct_colour(chl, ant) for ant in axes[1]] for chl in axes[0]])
    assert np.isfinite(values).all()
    return values


def lookup(values, pigments):
    pigments = np.asarray(pigments, float)
    size = len(values)
    if values.shape != (size, size, 6) or not 3 <= size <= 65:
        raise ValueError("bounded square six-channel pigment table required")
    if pigments.ndim != 2 or pigments.shape[1] != 2 or len(pigments) > 262144:
        raise ValueError("bounded Nx2 pigment coordinates required")
    if (
        not np.isfinite(pigments).all()
        or np.any(pigments < 0)
        or np.any(pigments > LIMITS)
    ):
        raise ValueError("pigment coordinates outside table domain")
    scaled = np.log1p(pigments) / np.log1p(LIMITS) * (size - 1)
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
