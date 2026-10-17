"""Numerical control assertions independent of optional providers and SDK oracles."""

import numpy as np


def near(actual, expected, tolerance=1e-8):
    if not np.allclose(actual, expected, rtol=0.0, atol=tolerance):
        raise AssertionError(f"{actual} != {expected}")
