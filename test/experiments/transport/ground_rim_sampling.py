"""Bounded uniform/adaptive observations on four fixed local rectangle edges."""

from dataclasses import dataclass, field

import numpy as np


@dataclass(frozen=True)
class RimRecipe:
    version: str = field(default="outshine/local-fitted-terrain-rim/1", init=False)
    sampling: str = "uniform"
    segments_per_side: int = 16
    maximum_vertices: int = 64
    maximum_dem_points: int = 128
    midpoint_error_m: float = 0.25
    maximum_adjustment_m: float = 2.0

    def __post_init__(self):
        if (
            self.sampling not in ("uniform", "adaptive")
            or type(self.segments_per_side) is not int
            or not 1 <= self.segments_per_side <= 16
            or type(self.maximum_vertices) is not int
            or not 4 <= self.maximum_vertices <= 64
            or type(self.maximum_dem_points) is not int
            or not 4 <= self.maximum_dem_points <= 256
            or not np.isfinite(self.midpoint_error_m)
            or not 0 < self.midpoint_error_m <= 8
            or not np.isfinite(self.maximum_adjustment_m)
            or not 0 < self.maximum_adjustment_m <= 8
        ):
            raise ValueError(
                "Bounded explicit rim sampling and DEM adjustment recipe required"
            )
        if (
            self.sampling == "uniform"
            and 4 * self.segments_per_side > self.maximum_vertices
        ):
            raise ValueError("Uniform rim vertex budget")


def sample_values(sample_z, xy):
    values = np.asarray(sample_z(xy), dtype=float)
    if values.shape != (len(xy),) or not np.isfinite(values).all():
        raise ValueError("missing_or_invalid_rim_dem")
    return values


def observe_rim(corners, heights, sample_z, recipe):
    if recipe.sampling == "uniform":
        count = recipe.segments_per_side
        xy = np.vstack(
            [
                a + (b - a) * np.arange(count)[:, None] / count
                for a, b in zip(corners, np.roll(corners, -1, axis=0), strict=True)
            ]
        )
        if len(xy) > recipe.maximum_dem_points:
            raise ValueError("Rim DEM observation budget")
        z = np.empty(len(xy))
        z[::count] = heights
        unknown = np.flatnonzero(np.arange(len(xy)) % count)
        if len(unknown):
            z[unknown] = sample_values(sample_z, xy[unknown])
        return (
            xy,
            z,
            {
                "method": "uniform",
                "dem_points": len(xy),
                "vertices": len(xy),
                "reason": "fixed_family",
            },
        )
    return adaptive_rim(corners, heights, sample_z, recipe)


def adaptive_rim(corners, heights, sample_z, recipe):
    edges = [
        (a, b, float(za), float(zb))
        for a, b, za, zb in zip(
            corners,
            np.roll(corners, -1, axis=0),
            heights,
            np.roll(heights, -1),
            strict=True,
        )
    ]
    if recipe.maximum_dem_points < 8:
        raise ValueError("Rim DEM observation budget")
    middle = (corners + np.roll(corners, -1, axis=0)) / 2
    values = sample_values(sample_z, middle)
    segments = [
        (*edge, mid, float(z))
        for edge, mid, z in zip(edges, middle, values, strict=True)
    ]
    queried, reason = 8, "midpoint_target_met"
    while True:
        errors = [abs(z - (za + zb) / 2) for _a, _b, za, zb, _mid, z in segments]
        chosen = int(np.argmax(errors))
        if errors[chosen] <= recipe.midpoint_error_m:
            break
        if (
            len(segments) >= recipe.maximum_vertices
            or queried + 2 > recipe.maximum_dem_points
        ):
            reason = "sampling_budget_exhausted"
            break
        a, b, za, zb, mid, zm = segments[chosen]
        if np.array_equal(mid, a) or np.array_equal(mid, b):
            reason = "sampling_precision_end"
            break
        left, right = (a + mid) / 2, (mid + b) / 2
        lo, hi = sample_values(sample_z, np.array([left, right]))
        queried += 2
        segments[chosen : chosen + 1] = [
            (a, mid, za, zm, left, float(lo)),
            (mid, b, zm, zb, right, float(hi)),
        ]
    return (
        np.array([s[0] for s in segments]),
        np.array([s[2] for s in segments]),
        {
            "method": "adaptive",
            "dem_points": queried,
            "vertices": len(segments),
            "reason": reason,
            "maximum_tested_midpoint_error_m": max(errors),
            "scope": "Midpoint-driven observation heuristic, not a continuous DEM supremum/error certificate; unresolved narrow features remain possible.",
        },
    )
