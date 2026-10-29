"""Bounded affine construction, separate from original DEM/profile admission.

The LP minimizes sampled maximum cut/fill inside a regular polygon contained in
the requested gradient disk. This supplies a conservative construction candidate,
not the global optimum over the disk or a continuous terrain-error certificate.
"""

import math
from dataclasses import dataclass
from fractions import Fraction

import numpy as np
from scipy.optimize import linprog


@dataclass(frozen=True)
class ConstructivePlane:
    origin: tuple
    intercept: float
    gradient: tuple

    def __post_init__(self):
        for name in ("origin", "gradient"):
            values = tuple(float(v) for v in getattr(self, name))
            if len(values) != 2 or not all(map(math.isfinite, values)):
                raise ValueError("Finite two-dimensional construction frame required")
            object.__setattr__(self, name, values)
        if not math.isfinite(self.intercept):
            raise ValueError("Finite constructed plane height required")

    def evaluate(self, xy):
        xy = np.asarray(xy, dtype=float)
        if xy.ndim != 2 or xy.shape[1] != 2 or not np.isfinite(xy).all():
            raise ValueError("Finite Nx2 construction positions required")
        return self.intercept + (xy - self.origin) @ np.asarray(self.gradient)


def observations(xy, heights, most):
    if (
        not isinstance(xy, np.ndarray)
        or not isinstance(heights, np.ndarray)
        or xy.ndim != 2
        or xy.shape[1] != 2
        or heights.shape != (len(xy),)
        or xy.dtype.kind != "f"
        or heights.dtype.kind != "f"
        or type(most) is not int
        or not 1 <= most <= 4096
        or not 1 <= len(xy) <= most
    ):
        raise ValueError("Bounded floating-point construction observations required")
    if (
        not np.isfinite(xy).all()
        or not np.isfinite(heights).all()
        or np.any(abs(xy) > 1e6)
        or np.any(abs(heights) > 1e6)
    ):
        raise ValueError("Finite bounded metric construction frame required")
    return xy.astype(float, copy=False), heights.astype(float, copy=False)


def affine_prior_conflict(xy, heights, gradient, prior, *, observations_most=512):
    xy, heights = observations(xy, heights, observations_most)
    if (
        not math.isfinite(gradient)
        or not 0 < gradient <= 1
        or not math.isfinite(prior)
        or prior < 0
    ):
        raise ValueError("Positive grade and nonnegative original prior required")
    best, pair = -math.inf, None
    for i in range(len(xy)):
        gap = abs(heights[i] - heights) - gradient * np.linalg.norm(xy[i] - xy, axis=1)
        j = int(np.argmax(gap))
        if gap[j] > best:
            best, pair = float(gap[j]), (i, j)
    i, j = pair
    height = abs(Fraction(float(heights[i])) - Fraction(float(heights[j])))
    remaining = height - 2 * Fraction(float(prior))
    distance2 = sum(
        (Fraction(float(a)) - Fraction(float(b))) ** 2
        for a, b in zip(xy[i], xy[j], strict=True)
    )
    if remaining <= 0 or remaining**2 <= Fraction(float(gradient)) ** 2 * distance2:
        return None
    return {
        "indices": pair,
        "prior_m": prior,
        "gradient_cap": gradient,
        "scope": "Exact rational contradiction for these finite observations and a globally affine plane. No source-datum or general nonconvex piecewise-field infeasibility claim.",
    }


def fit_plane(
    xy,
    heights,
    *,
    maximum_gradient,
    maximum_earthwork_m,
    time_limit_s=0.5,
    directions=64,
    observations_most=512,
):
    xy, heights = observations(xy, heights, observations_most)
    if (
        not math.isfinite(maximum_gradient)
        or not 0 < maximum_gradient <= 1
        or not math.isfinite(maximum_earthwork_m)
        or not 0 <= maximum_earthwork_m <= 1000
        or not math.isfinite(time_limit_s)
        or not 0 < time_limit_s <= 60
        or type(directions) is not int
        or not 8 <= directions <= 256
    ):
        raise ValueError("Bounded constructive grade, earthworks and LP work required")
    order = np.lexsort((heights, xy[:, 1], xy[:, 0]))
    xy, heights = xy[order], heights[order]
    same = np.all(xy[1:] == xy[:-1], axis=1)
    if np.any(same & (heights[1:] != heights[:-1])):
        raise ValueError("Conflicting original observations at the same position")
    keep = np.r_[True, ~same]
    xy, heights = xy[keep], heights[keep]
    origin = xy.mean(axis=0)
    design = np.c_[np.ones(len(xy)), xy - origin]
    angles = np.arange(directions) * (2 * np.pi / directions)
    normals = np.c_[np.cos(angles), np.sin(angles)]
    matrix = np.vstack(
        (
            np.c_[design, -np.ones(len(xy))],
            np.c_[-design, -np.ones(len(xy))],
            np.c_[np.zeros(directions), normals, np.zeros(directions)],
        )
    )
    bounds = np.r_[
        heights,
        -heights,
        np.full(directions, maximum_gradient * math.cos(math.pi / directions)),
    ]
    result = linprog(
        [0, 0, 0, 1],
        A_ub=matrix,
        b_ub=bounds,
        bounds=[(None, None)] * 3 + [(0, None)],
        method="highs",
        options={"time_limit": time_limit_s},
    )
    report = {
        "state": "pending",
        "reason": "plane_lp_unavailable",
        "solver_status": int(result.status),
        "observations": len(xy),
        "constraint_rows": len(matrix),
        "gradient_directions": directions,
        "scope": "Constructive sampled cut/fill; original profiles and DEM-fit/Source/World admission remain separate. No exact disk optimum or continuous terrain certificate.",
    }
    if not result.success:
        return None, report
    if result.x.shape != (4,) or not np.isfinite(result.x).all():
        return None, report | {"reason": "nonfinite_plane_candidate"}
    plane = ConstructivePlane(tuple(origin), float(result.x[0]), tuple(result.x[1:3]))
    error = float(abs(plane.evaluate(xy) - heights).max())
    grade = math.hypot(*plane.gradient)
    report.update(maximum_observed_cut_fill_m=error, maximum_constructed_grade=grade)
    if grade > maximum_gradient * (1 + 1e-9):
        return None, report | {"reason": "unqualified_plane_gradient"}
    if error > maximum_earthwork_m:
        return None, report | {"reason": "construction_earthwork_budget"}
    return plane, report | {"state": "candidate", "reason": ""}
