"""Immutable Bernstein curves and prepared scalar profiles for generator properties."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from scipy.interpolate import BPoly
from scipy.optimize import brentq, elementwise


def finite(values):
    result = np.asarray(values, dtype=float)
    if not np.all(np.isfinite(result)):
        raise ValueError("finite profile coordinates required")
    return result


@dataclass(frozen=True)
class BezierCurve:
    controls: tuple

    def __post_init__(self):
        points = finite(self.controls)
        if points.ndim != 3 or points.shape[1:] != (4, 2) or not 1 <= len(points) <= 64:
            raise ValueError("one to 64 cubic two-dimensional segments required")
        if not np.array_equal(points[:-1, -1], points[1:, 0]):
            raise ValueError("continuous curve endpoints required")
        object.__setattr__(
            self, "controls", tuple(tuple(tuple(p) for p in c) for c in points)
        )

    @cached_property
    def polynomial(self):
        return BPoly(
            np.asarray(self.controls).transpose(1, 0, 2),
            np.linspace(0, 1, len(self.controls) + 1),
        )

    def raw(self, parameter):
        return self.polynomial(np.clip(finite(parameter), 0, 1))

    def coordinates(self, coordinate):
        points = np.asarray(self.controls)
        if points[0, 0, 0] != 0 or points[-1, -1, 0] != 1:
            raise ValueError("profile endpoints must span zero to one")
        derivative = 3 * np.diff(points[..., 0], axis=1)
        a = derivative[:, 0] - 2 * derivative[:, 1] + derivative[:, 2]
        u = np.clip(
            np.divide(
                derivative[:, 0] - derivative[:, 1],
                a,
                out=np.zeros_like(a),
                where=a > 0,
            ),
            0,
            1,
        )
        minimum = np.minimum(derivative[:, 0], derivative[:, 2])
        interior = (
            derivative[:, 0] * (1 - u) ** 2
            + 2 * derivative[:, 1] * u * (1 - u)
            + derivative[:, 2] * u**2
        )
        minimum = np.minimum(minimum, interior)
        if np.any(minimum < 0) or np.any(points[:, -1, 0] <= points[:, 0, 0]):
            raise ValueError("single-valued monotone profile required")
        return np.clip(finite(coordinate), 0, 1)

    def reference_value(self, coordinate):
        x = self.coordinates(coordinate)
        parameters = [
            brentq(
                lambda t, target=target: float(self.polynomial(t)[0] - target),
                0,
                1,
                xtol=1e-13,
            )
            for target in x.ravel()
        ]
        return self.polynomial(np.asarray(parameters).reshape(x.shape))[..., 1]

    def value(self, coordinate):
        x = self.coordinates(coordinate)
        result = elementwise.find_root(
            lambda t, targets: self.polynomial(t)[..., 0] - targets,
            (np.zeros_like(x), np.ones_like(x)),
            args=(x,),
            tolerances={"xatol": 1e-13, "fatol": 1e-14},
            maxiter=100,
        )
        if not np.all(result.success):
            raise ValueError("profile inverse did not converge within its budget")
        return self.polynomial(result.x)[..., 1]


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
        table = table.copy()
        table.setflags(write=False)
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
