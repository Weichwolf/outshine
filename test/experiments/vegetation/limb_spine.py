"""Bounded C1 composition of existing regular 64-segment cubic spines."""

import numpy as np
from outshine_ref.vegetation import CubicSpine


class LimbSpine:
    def __init__(self, points, derivatives):
        if not 2 <= len(points) <= 1025:
            raise ValueError("two through1025 limb stations required")
        self.edges = len(points) - 1
        self.parts = []
        for begin in range(0, self.edges, 64):
            end = min(begin + 64, self.edges)
            part = CubicSpine.from_hermite(
                points[begin : end + 1], derivatives[begin : end + 1] * (end - begin)
            )
            self.parts.append(part)

    def evaluate(self, parameter, derivative=0):
        parameter = np.asarray(parameter, float)
        if (
            not np.isfinite(parameter).all()
            or np.any((parameter < 0) | (parameter > 1))
            or type(derivative) is not int
            or not 0 <= derivative <= 2
        ):
            raise ValueError("normalized finite parameters and derivative0..2 required")
        flat = parameter.ravel()
        owner = np.minimum((flat * self.edges / 64).astype(int), len(self.parts) - 1)
        result = np.empty((len(flat), 3))
        for index, part in enumerate(self.parts):
            rows = owner == index
            begin, end = index * 64, min((index + 1) * 64, self.edges)
            scale = self.edges / (end - begin)
            local = np.clip((flat[rows] * self.edges - begin) / (end - begin), 0, 1)
            result[rows] = part.evaluate(local, derivative) * scale**derivative
        return result.reshape(parameter.shape + (3,))

    def sample(self, parameter):
        # The existing geometric derivative calculation also applies to this composition.
        return CubicSpine.sample(self, parameter)
