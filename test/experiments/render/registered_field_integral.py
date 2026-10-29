"""Decode the explicit native paired-integral readback without discarding low bits."""

import numpy as np

INTEGRAL_ENCODING = "rgba32f-hi-lo-pairs-v1"


def decode_integral(packed, report, width, height):
    shape = (height + 1, 2 * (width + 1), 4)
    if (
        width <= 0
        or height <= 0
        or report.get("integral_encoding") != INTEGRAL_ENCODING
        or report.get("integral_width") != shape[1]
        or report.get("integral_height") != shape[0]
        or packed.shape != shape
        or packed.dtype != np.float32
        or not np.isfinite(packed).all()
    ):
        raise ValueError("native paired integral encoding or extent mismatch")
    if np.any(packed[0]) or np.any(packed[:, :2]):
        raise ValueError("paired integral border is not transparent zero")
    return packed[:, 0::2].astype(np.float64) + packed[:, 1::2].astype(np.float64)

