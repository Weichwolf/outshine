"""Private prepared SciPy data stays immutable; public inspection owns a copy."""

import numpy as np


def freeze_polynomial(polynomial):
    for name in ("c", "x"):
        values = getattr(polynomial, name)
        setattr(
            polynomial,
            name,
            np.frombuffer(values.tobytes(), dtype=values.dtype).reshape(values.shape),
        )
    return polynomial


def copy_polynomial(polynomial):
    return type(polynomial).construct_fast(
        polynomial.c.copy(),
        polynomial.x.copy(),
        extrapolate=polynomial.extrapolate,
        axis=polynomial.axis,
    )
