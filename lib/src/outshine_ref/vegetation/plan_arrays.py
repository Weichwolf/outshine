"""Bounded detail requests and immutable shared plant-plan arrays."""

import numpy as np

MAX_DETAIL = 100_000


def immutable_array(values):
    values = np.asarray(values)
    return np.frombuffer(values.tobytes(), dtype=values.dtype).reshape(values.shape)


def detail_budget(value):
    if type(value) is not int or not 0 <= value <= MAX_DETAIL:
        raise ValueError("integer detail budget between zero and 100000 required")
    return value
