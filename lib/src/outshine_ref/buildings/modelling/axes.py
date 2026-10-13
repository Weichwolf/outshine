"""Rigid conversion of positions/directions; triangle winding is preserved."""

import numpy as np
from numpy.typing import ArrayLike, NDArray


def _coordinates(values: ArrayLike) -> NDArray[np.float64]:
    array = np.asarray(values)
    if array.ndim < 1 or array.shape[-1] != 3 or array.dtype.kind not in "fiu":
        raise ValueError("coordinates require real numeric vectors with three components")
    array = np.asarray(array, dtype=np.float64)
    if not np.isfinite(array).all():
        raise ValueError("coordinates must be finite")
    return array


def cga_to_enu(values: ArrayLike) -> NDArray[np.float64]:
    """East/up/south → east/north/up; also applies to unit normal vectors."""
    return _coordinates(values)[..., [0, 2, 1]] * [1., -1., 1.]


def enu_to_cga(values: ArrayLike) -> NDArray[np.float64]:
    """East/north/up → east/up/south; units and local origin remain unchanged."""
    return _coordinates(values)[..., [0, 2, 1]] * [1., 1., -1.]
