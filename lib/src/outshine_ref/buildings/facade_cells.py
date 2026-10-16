"""Bounded rectangular complement of explicit existing facade cells, without cell expansion."""

from .facades import FacadeRepeat


def cell_complement(repeat, excluded, *, max_excluded=128):
    if not isinstance(repeat, FacadeRepeat):
        raise TypeError("An existing FacadeRepeat is required")
    if type(max_excluded) is not int or not 0 <= max_excluded <= 128:
        raise ValueError("Cell exclusion capacity requires0..128")
    if not isinstance(excluded, tuple) or len(excluded) > max_excluded:
        raise ValueError("Immutable exclusions exceed the declared capacity")
    for cell in excluded:
        if (
            not isinstance(cell, tuple)
            or len(cell) != 2
            or any(type(v) is not int for v in cell)
        ):
            raise ValueError("Existing cell identities require immutable integer pairs")
        if not 0 <= cell[0] < repeat.columns or not 0 <= cell[1] < repeat.rows:
            raise ValueError("Excluded cell is outside its existing facade")
    if len(set(excluded)) != len(excluded):
        raise ValueError("An existing cell may only be excluded once")
    ranges = [(0, 0, repeat.columns, repeat.rows)]
    for c, r in sorted(excluded):
        index = next(
            i
            for i, (x0, y0, x1, y1) in enumerate(ranges)
            if x0 <= c < x1 and y0 <= r < y1
        )
        x0, y0, x1, y1 = ranges.pop(index)
        parts = [
            (x0, y0, c, y1),
            (c + 1, y0, x1, y1),
            (c, y0, c + 1, r),
            (c, r + 1, c + 1, y1),
        ]
        ranges.extend(p for p in parts if p[0] < p[2] and p[1] < p[3])
    return tuple(sorted(ranges))
