"""Fixed last-axis 3D products using NumPy component ufuncs, without axis dispatch."""

import numpy as np


def cross3(left, right):
    left, right = np.asarray(left), np.asarray(right)
    if left.shape[-1:] != (3,) or right.shape[-1:] != (3,):
        raise ValueError("three-dimensional last-axis vectors required")
    dtype = np.result_type(left.dtype, right.dtype)
    if dtype.kind not in "fiuc":
        raise ValueError("real or complex numeric vectors required")
    left, right = left.astype(dtype, copy=False), right.astype(dtype, copy=False)
    return np.stack(
        (
            left[..., 1] * right[..., 2] - left[..., 2] * right[..., 1],
            left[..., 2] * right[..., 0] - left[..., 0] * right[..., 2],
            left[..., 0] * right[..., 1] - left[..., 1] * right[..., 0],
        ),
        axis=-1,
    )
