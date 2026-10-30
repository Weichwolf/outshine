"""Source-edge bay axes from the actual final upper facade plan."""

import math
from bisect import bisect_left
from itertools import pairwise

import numpy as np


class FacadeBays:
    def __init__(self, frames, *, method="bisect"):
        if method not in ("bisect", "linear") or not frames:
            raise ValueError("Explicit upper facade frames and bay selection required")
        self.method, self.frames = method, {}
        columns, identifiers = 0, set()
        for frame in frames:
            a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
            width, count = frame["width_m"], frame["columns"]
            if (
                a.shape != (2,)
                or b.shape != (2,)
                or not np.isfinite([a, b]).all()
                or not math.isfinite(width)
                or width <= 0
                or type(count) is not int
                or not 1 <= count <= 4096
                or abs(np.linalg.norm(b - a) - width) > 1e-7
                or not isinstance(frame["id"], str)
                or not frame["id"]
            ):
                raise ValueError(
                    "Upper facade needs finite source edges and bounded bays"
                )
            key = tuple(a), tuple(b)
            if key in self.frames or frame["id"] in identifiers:
                raise ValueError("Source edge has multiple upper facade owners")
            columns += count
            if columns > 4096:
                raise ValueError("Upper facade bay capacity exceeded")
            cell = width / count
            centres = tuple((i + 0.5) * cell for i in range(count))
            if any(x >= y for x, y in pairwise(centres)):
                raise ValueError(
                    "Upper facade bay coordinates are not strictly ordered"
                )
            self.frames[key] = frame["id"], centres
            identifiers.add(frame["id"])

    def nearest(self, start, end, along):
        try:
            identifier, centres = self.frames[tuple(start), tuple(end)]
        except KeyError as error:
            raise ValueError(
                "Source roof edge has no exact upper facade owner"
            ) from error
        if not math.isfinite(along):
            raise ValueError("Finite source-edge position required")
        if along <= centres[0]:
            choices = (0,)
        elif along >= centres[-1]:
            choices = (len(centres) - 1,)
        elif self.method == "linear":
            choices = range(len(centres))
        else:
            at = bisect_left(centres, along)
            choices = range(max(0, at - 1), min(len(centres), at + 1))
        column = min(choices, key=lambda i: (abs(centres[i] - along), i))
        return identifier, column, centres[column]
