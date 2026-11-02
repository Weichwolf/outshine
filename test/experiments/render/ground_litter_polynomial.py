"""Closed-form moments of the existing 3x3 Gauss filter within one noise cell."""

import numpy as np


def cell_weights(at, dx, dy):
    at, dx, dy = (np.asarray(v, dtype=float) for v in (at, dx, dy))
    if (
        at.ndim != 2
        or at.shape[1] != 2
        or dx.shape != at.shape
        or dy.shape != at.shape
        or not np.isfinite((at, dx, dy)).all()
    ):
        raise ValueError("matching finite two-dimensional cell queries required")
    f = at - np.floor(at)
    extent = np.sqrt(0.15) * (np.abs(dx) + np.abs(dy))
    inside = np.all((f - extent >= 0) & (f + extent < 1), axis=1)
    a, c = dx.T
    b, d = dy.T
    a2, b2, c2, d2 = a * a, b * b, c * c, d * d
    v = (dx * dx + dy * dy) / 12
    uv = (a * c + b * d) / 12
    uuvv = (a2 * c2 + b2 * d2) / 80 + (a2 * d2 + 4 * a * b * c * d + b2 * c2) / 144
    uvvv = (a * c * c2 + b * d * d2) / 80 + (a * c * d2 + b * c2 * d) / 48
    uuuv = (a * a2 * c + b * b2 * d) / 80 + (a2 * b * d + a * b2 * c) / 48
    uuuvvv = (a * a2 * c * c2 + b * b2 * d * d2) * (3 / 1600) + (
        a * a2 * c * d2
        + a * b2 * c * c2
        + a2 * b * d * d2
        + b * b2 * c2 * d
        + 3 * a2 * b * c2 * d
        + 3 * a * b2 * c * d2
    ) / 320
    h = f * f * (3 - 2 * f)
    first = 6 * f * (1 - f)
    second = 3 - 6 * f
    mean = h + second * v
    mixed = (
        h[:, 0] * h[:, 1]
        + h[:, 0] * second[:, 1] * v[:, 1]
        + second[:, 0] * h[:, 1] * v[:, 0]
        + first[:, 0] * first[:, 1] * uv
        + second[:, 0] * second[:, 1] * uuvv
        - 2 * first[:, 0] * uvvv
        - 2 * first[:, 1] * uuuv
        + 4 * uuuvvv
    )
    return inside, np.column_stack(
        (
            1 - mean[:, 0] - mean[:, 1] + mixed,
            mean[:, 0] - mixed,
            mean[:, 1] - mixed,
            mixed,
        )
    )
