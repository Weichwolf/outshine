"""Owned immutable prepared profiles; lookup requires NumPy, not curve solvers."""

from dataclasses import dataclass

import numpy as np


def finite(values):
    result = np.asarray(values)
    if result.dtype.kind not in "fiu":
        raise ValueError("real numeric profile coordinates required")
    result = np.asarray(result, dtype=float)
    if not np.all(np.isfinite(result)):
        raise ValueError("finite profile coordinates required")
    return result


@dataclass(frozen=True, eq=False)
class Profile:
    table: np.ndarray
    kind: str = "custom"

    def __post_init__(self):
        table = finite(self.table)
        if table.ndim != 1 or not 4 <= len(table) <= 4096:
            raise ValueError("four to 4096 prepared scalar values required")
        shapes = {
            "maximum": np.ones(len(table)),
            "minimum": np.zeros(len(table)),
            "linear": np.linspace(0, 1, len(table)),
            "inverse_linear": np.linspace(1, 0, len(table)),
        }
        if self.kind != "custom" and (
            self.kind not in shapes
            or not np.allclose(table, shapes[self.kind], rtol=0, atol=1e-14)
        ):
            raise ValueError("declared profile preset must match the stored values")
        table = np.frombuffer(table.tobytes(), dtype=np.float64)
        object.__setattr__(self, "table", table)

    @classmethod
    def from_curve(cls, curve, size=1000):
        if type(size) is not int or not 4 <= size <= 4096:
            raise ValueError("bounded integer profile table size required")
        return cls(curve.value(np.linspace(0, 1, size)))

    def evaluate(self, coordinate, *, tiled=False, extra=None, clamp=False):
        x = finite(coordinate)
        if self.kind != "custom":
            value = {
                "maximum": np.ones_like(x),
                "minimum": np.zeros_like(x),
                "linear": x if tiled else np.clip(x, 0, 1),
                "inverse_linear": 1 - (x if tiled else np.clip(x, 0, 1)),
            }[self.kind]
            if tiled and self.kind in ("maximum", "minimum"):
                return value
            if not tiled and extra is not None:
                value += extra.evaluate(x)
            return np.clip(value, 0, 1) if clamp else value
        if tiled:
            cycle = np.floor(x)
            fraction = x - cycle
        else:
            cycle = 0
            fraction = np.clip(x, 0, 1)
        value = np.interp(fraction, np.linspace(0, 1, len(self.table)), self.table)
        if extra is not None:
            value += extra.evaluate(fraction)
        drift = self.table[-1] - self.table[0]
        if extra is not None:
            drift += extra.table[-1] - extra.table[0]
        value += cycle * drift
        return np.clip(value, 0, 1) if clamp else value

    def derivative(self, coordinate):
        x = finite(coordinate)
        index = np.minimum(
            (np.clip(x, 0, 1) * (len(self.table) - 1)).astype(int), len(self.table) - 2
        )
        slope = (self.table[index + 1] - self.table[index]) * (len(self.table) - 1)
        return np.where((x < 0) | (x > 1), 0, slope)

    def bytes(self):
        return self.table.nbytes
