"""Three source-stable vertical scopes with distinct metrical window programmes."""

import math
from dataclasses import dataclass

import numpy as np
from outshine_ref.buildings.facades import FacadeRepeat


@dataclass(frozen=True)
class Scope:
    name: str
    base_m: float
    top_m: float
    rows: int
    columns: int
    fractions: tuple


@dataclass(frozen=True)
class Elevation:
    facade_id: str
    kind: str
    a: tuple
    b: tuple
    width_m: float
    scopes: tuple

    def frames(self):
        return tuple(
            {
                "id": self.facade_id + "/scope/" + s.name,
                "source_facade_id": self.facade_id,
                "architecture_scope": s.name,
                "base_m": s.base_m,
                "top_m": s.top_m,
                "a": list(self.a),
                "b": list(self.b),
                "width_m": self.width_m,
                "height_m": s.top_m - s.base_m,
                "columns": s.columns,
                "rows": s.rows,
                "fractions": list(s.fractions),
                "opening_count": s.columns * s.rows,
            }
            for s in self.scopes
        )


def plan(facade, bottom, kind, seed):
    if kind not in ("horizontal-ribbons", "vertical-punched"):
        raise ValueError("Known elevation programme required")
    width, height = facade["width_m"], facade["height_m"]
    levels = facade["rows"]
    a, b = np.asarray(facade["a"]), np.asarray(facade["b"])
    if (
        not np.isfinite([width, height, bottom]).all()
        or not np.isfinite([a, b]).all()
        or width < 12
        or not 3 <= levels <= 16
        or abs(np.linalg.norm(b - a) - width) > 1e-7
        or height / levels < 2.64
    ):
        raise ValueError("Elevation requires resolved12m width and3..16 safe storeys")
    floor = height / levels
    base_height = max(3.0, floor * 1.12)
    top_height = max(2.64, floor * 0.90)
    body_height = height - base_height - top_height
    if body_height / (levels - 2) < 2.64:
        raise ValueError("Distinct base/top leave no safe main-storey height")
    scale = 0.90 + 0.05 * (seed % 5)
    if kind == "horizontal-ribbons":
        axes = (3.6, 6.2, 5.0)
        fractions = ((0.58, 0.65), (0.90, 0.70), (0.82, 0.36))
    else:
        axes = (3.6, 2.6, 4.5)
        fractions = ((0.50, 0.62), (0.42, 0.76), (0.68, 0.48))
    bases = (bottom, bottom + base_height, bottom + height - top_height)
    tops = (*bases[1:], bottom + height)
    counts = (1, levels - 2, 1)
    scopes = []
    for name, low, high, rows, axis, sizes in zip(
        ("base", "body", "top"), bases, tops, counts, axes, fractions, strict=True
    ):
        columns = max(1, math.floor(width / (axis * scale) + 0.5))
        repeat = FacadeRepeat(width, high - low, columns, rows, *sizes)
        cell, storey = repeat.cell_size_m
        if min(cell * sizes[0], storey * sizes[1]) < 0.6:
            raise ValueError("Scope leaves no useful opening")
        scopes.append(Scope(name, low, high, rows, columns, sizes))
    return Elevation(
        facade["id"], kind, tuple(a), tuple(b), float(width), tuple(scopes)
    )
