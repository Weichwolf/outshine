"""Bounded immutable Gauss-Legendre data shared by organ and axis integration."""

from functools import lru_cache

import numpy as np


@lru_cache(maxsize=16, typed=True)
def quadrature(order):
    if (
        isinstance(order, (bool, np.bool_))
        or not isinstance(order, (int, np.integer))
        or not 1 <= order <= 1024
    ):
        raise ValueError("bounded integer quadrature order required")
    nodes, weights = np.polynomial.legendre.leggauss(order)
    nodes = np.frombuffer(nodes.tobytes(), dtype=np.float64)
    weights = np.frombuffer(weights.tobytes(), dtype=np.float64)
    return nodes, weights
