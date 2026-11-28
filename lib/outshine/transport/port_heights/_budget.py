"""Explicit offline work limits; cooperative steps may overrun their time request."""

from dataclasses import dataclass, fields
from math import isfinite


@dataclass(frozen=True, slots=True)
class PortHeightBudget:
    """Bound input, sparse problem size, geometry, sampling and cooperative time.

    Limits count work, not reserved process memory. A hard address-space/CPU cap
    belongs to the worker supervising the offline call. Solver factorization and
    bounded GEOS/NumPy phases may finish after their cooperative deadline.
    """

    time_limit_s: float = 90.0
    solver_time_limit_s: float = 8.0
    geometry_time_limit_s: float = 15.0
    max_iterations: int = 100
    max_rounds: int = 6
    max_fits: int = 3
    initial_segments: int = 8
    max_vertices: int = 65536
    max_faces: int = 65536
    max_parts: int = 8192
    max_ports: int = 16384
    max_output_vertices: int = 131072
    max_output_faces: int = 262144
    max_stations_per_part: int = 512
    max_weld_pairs: int = 1048576
    max_queries: int = 262144
    max_cones: int = 300000
    max_variables: int = 8192
    max_sparse_nonzeros: int = 12000000

    def __post_init__(self):
        for field in fields(self):
            value = getattr(self, field.name)
            if field.name.endswith("_s"):
                if not isfinite(value) or not 0 <= value <= 3600:
                    raise ValueError("finite cooperative seconds in [0,3600] required")
            elif type(value) is not int or not 0 < value <= 2**31 - 1:
                raise ValueError("positive bounded integer work limits required")
        if self.initial_segments > 64 or self.max_rounds > 64 or self.max_fits > 64:
            raise ValueError("at most 64 initial segments, rounds and fits required")
        if self.max_queries > 1048576:
            raise ValueError(
                "at most 1048576 analytic samples per bounded call required"
            )


class PortLimit(ValueError):
    """A work limit ended this local height plan without an unchecked product."""


DEFAULT_BUDGET = PortHeightBudget()
