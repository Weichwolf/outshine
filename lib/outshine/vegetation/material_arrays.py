"""Bounded real arrays shared by procedural material reference procedures."""

import numpy as np

MAX_TEXELS = 512 * 512


def real_array(value, channels=None):
    array = np.asarray(value)
    count = array.size if channels is None else array.size / channels
    if (
        array.dtype.kind not in "fiu"
        or count > MAX_TEXELS
        or channels is not None
        and array.shape[-1:] != (channels,)
        or not np.isfinite(array).all()
    ):
        raise ValueError("bounded finite real material array required")
    return np.asarray(array, dtype=np.float64)


def rows(value, channels):
    array = real_array(value, channels)
    if array.ndim != 2:
        raise ValueError("explicit material rows required")
    return array
