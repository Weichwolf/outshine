"""Finite physical stair schedules; no semantic level is converted to metres."""

import math
from dataclasses import dataclass

import numpy as np
import shapely


@dataclass(frozen=True)
class StairLimits:
    min_rise_m: float = 0.14
    max_rise_m: float = 0.18
    min_tread_m: float = 0.25
    max_tread_m: float = 0.35
    min_stride_m: float = 0.60
    max_stride_m: float = 0.66
    landing_m: float = 1.5
    waist_m: float = 0.2
    clearance_m: float = 0.001
    max_flight_risers: int = 16
    max_risers: int = 64
    max_flights: int = 8
    max_source_parts: int = 128

    def __post_init__(self):
        metrics = (
            self.min_rise_m,
            self.max_rise_m,
            self.min_tread_m,
            self.max_tread_m,
            self.min_stride_m,
            self.max_stride_m,
            self.landing_m,
            self.waist_m,
            self.clearance_m,
        )
        if not np.isfinite(metrics).all() or min(metrics) <= 0:
            raise ValueError("Finite positive construction limits required")
        if (
            self.min_rise_m > self.max_rise_m
            or self.min_tread_m > self.max_tread_m
            or self.min_stride_m > self.max_stride_m
            or self.waist_m < self.max_rise_m
        ):
            raise ValueError(
                "Ordered stair limits and a nonintersecting waist required"
            )
        if any(
            type(n) is not int or not 1 <= n <= 256
            for n in (
                self.max_flight_risers,
                self.max_risers,
                self.max_flights,
                self.max_source_parts,
            )
        ):
            raise ValueError("Bounded integer stair capacities required")


def schedule(length_m, rise_m, limits, method="enumerated"):
    if not np.isfinite((length_m, rise_m)).all() or min(length_m, rise_m) <= 0:
        raise ValueError("Positive metric stair length and rise required")
    if method not in ("greedy", "enumerated"):
        raise ValueError("Known physical stair search required")
    minimum = math.ceil(rise_m / limits.max_rise_m)
    maximum = min(limits.max_risers, math.floor(rise_m / limits.min_rise_m))
    candidates, tested = [], 0
    for risers in range(minimum, maximum + 1):
        fewest = math.ceil(risers / limits.max_flight_risers)
        for flights in range(fewest, min(risers, limits.max_flights) + 1):
            if risers == flights:
                continue
            tested += 1
            rise = rise_m / risers
            tread = (length_m - (flights + 1) * limits.landing_m) / (risers - flights)
            stride = 2 * rise + tread
            if (
                limits.min_tread_m <= tread <= limits.max_tread_m
                and limits.min_stride_m <= stride <= limits.max_stride_m
            ):
                # Prefer fewer flights; the remaining objective chooses a
                # comfortable recipe, then fewer risers within this family.
                score = (flights, abs(rise - 0.17) + abs(stride - 0.63), risers)
                candidates.append(
                    (
                        score,
                        {
                            "risers": risers,
                            "flights": flights,
                            "rise_m": rise,
                            "tread_m": tread,
                            "stride_m": stride,
                            "flight_risers": tuple(
                                risers // flights + int(i < risers % flights)
                                for i in range(flights)
                            ),
                        },
                    )
                )
            if method == "greedy":
                break
        if method == "greedy":
            break
    if not candidates:
        return None, {
            "candidates_tested": tested,
            "reason": "no_bounded_stair_schedule",
        }
    return min(candidates, key=lambda c: c[0])[1], {
        "candidates_tested": tested,
        "reason": "",
        "scope": "Finite balanced-flight schedules at fixed width/landing and supplied end heights; not a global layout optimum or building-code certificate.",
    }


def profile(length_m, height_m, recipe, limits):
    levels = np.linspace(0.0, height_m, recipe["risers"] + 1)
    points, bottom, done = [(0.0, 0.0)], [(0.0, -limits.waist_m)], 0
    for flight, count in enumerate(recipe["flight_risers"]):
        start = (flight + 1) * limits.landing_m + (done - flight) * recipe["tread_m"]
        points.append((start, float(levels[done])))
        bottom.append((start, float(levels[done]) - limits.waist_m))
        for riser in range(count):
            at = start + riser * recipe["tread_m"]
            points.append((at, float(levels[done + riser + 1])))
            if riser + 1 < count:
                points.append(
                    (
                        start + (riser + 1) * recipe["tread_m"],
                        float(levels[done + riser + 1]),
                    )
                )
        end = start + (count - 1) * recipe["tread_m"]
        bottom.append((end, float(levels[done + count - 1]) - limits.waist_m))
        done += count
    points.append((length_m, height_m))
    top = np.asarray(points)
    bottom.append((length_m, height_m - limits.waist_m))
    bottom = np.asarray(bottom)
    bottom = bottom[np.r_[True, np.any(np.diff(bottom, axis=0) != 0, axis=1)]]
    polygon = shapely.Polygon(np.concatenate((top, bottom[::-1])))
    if not polygon.is_valid or polygon.area <= 0:
        raise ValueError("invalid_stair_waist_profile")
    return top, bottom, polygon
