"""Bounded pigment interpolation over an external PROSPECT-D reference model."""

from functools import lru_cache

import colour
import numpy as np
import prosail
from outshine_ref.vegetation.leaf_optics import gamut_map, lookup

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


def build_table(size):
    if type(size) is not int or not 3 <= size <= 65:
        raise ValueError("3..65 samples per pigment dimension required")
    axes = [np.expm1(np.linspace(0, np.log1p(limit), size)) for limit in LIMITS]
    for axis, limit in zip(axes, LIMITS):
        axis[-1] = limit
    values = np.array([[direct_colour(chl, ant) for ant in axes[1]] for chl in axes[0]])
    assert np.isfinite(values).all()
    return values


__all__ = [
    "FIXED",
    "LIMITS",
    "build_table",
    "direct_colour",
    "gamut_map",
    "lookup",
    "rgb_weights",
]
