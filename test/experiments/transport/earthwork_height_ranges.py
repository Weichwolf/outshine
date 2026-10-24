"""Prepare constant RoadBedAt limits for immutable experimental terrain stamps."""

import math
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Stamp:
    ring: tuple
    at_e: float
    at_n: float
    plateau: float
    slope_e: float
    slope_n: float
    sag_inv: float = 0.0
    kind: str = "Corridor"
    corridor_key: int = 1

    def __post_init__(self):
        fields = ("at_e", "at_n", "plateau", "slope_e", "slope_n", "sag_inv")
        for name in fields:
            object.__setattr__(self, name, float(getattr(self, name)))
        ring = tuple(tuple(float(v) for v in p) for p in self.ring)
        if (
            any(len(p) != 2 for p in ring)
            or not all(math.isfinite(v) for p in ring for v in p)
            or not all(
                math.isfinite(v) for v in (getattr(self, name) for name in fields)
            )
            or type(self.corridor_key) is not int
            or not 0 <= self.corridor_key < 2**64
            or self.kind not in ("Corridor", "Pad", "Clearance", "Basin")
        ):
            raise ValueError("finite immutable non-profiled stamp required")
        object.__setattr__(self, "ring", ring)

    def wants_at(self, at):
        de, dn = at[0] - self.at_e, at[1] - self.at_n
        return (
            self.plateau
            + self.slope_e * de
            + self.slope_n * dn
            - 0.5 * (de * de + dn * dn) * self.sag_inv
        )

    @property
    def clamps(self):
        return (
            self.kind == "Corridor" and self.corridor_key != 0 and len(self.ring) >= 3
        )


def _limits(stamp):
    low, high = 1e30, -1e30
    for corner in stamp.ring:
        height = stamp.wants_at(corner)
        if not math.isfinite(height):
            raise ValueError("finite ring height required")
        low, high = min(low, height), max(high, height)
    return low, high


def direct(stamp, at):
    """Keep the current native operation order, including per-query ring visits."""
    bed = stamp.wants_at(at)
    if not stamp.clamps:
        return bed
    low, high = _limits(stamp)
    return min(max(bed, low), high)


@dataclass(frozen=True, slots=True)
class PreparedRange:
    stamp: Stamp
    limits: tuple | None

    def at(self, point):
        bed = self.stamp.wants_at(point)
        if self.limits is None:
            return bed
        low, high = self.limits
        return min(max(bed, low), high)


def prepare(stamps, *, max_stamps=8192, max_ring_points=65536, cancelled=None):
    """Retain immutable stamp identities; publish a complete bounded table only."""
    if any(type(v) is not int or v < 0 for v in (max_stamps, max_ring_points)):
        raise ValueError("nonnegative preparation budgets required")
    result, vertices = [], 0
    for stamp in stamps:
        if cancelled is not None and cancelled():
            raise ValueError("height-range preparation cancelled")
        if not isinstance(stamp, Stamp):
            raise TypeError("immutable stamp required")
        vertices += len(stamp.ring)
        if len(result) >= max_stamps or vertices > max_ring_points:
            raise ValueError("height-range preparation budget exhausted")
        result.append(PreparedRange(stamp, _limits(stamp) if stamp.clamps else None))
    if cancelled is not None and cancelled():
        raise ValueError("height-range preparation cancelled")
    return tuple(result)
