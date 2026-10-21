"""Independent four-variable LP and exact feasible point reconstruction; off-frame only."""

from fractions import Fraction

import numpy as np
from scipy.optimize import linprog


def penetration_witness(triangle, start, end, lower, upper):
    """Maximize box-face slack over time and triangle barycentric coordinates.

    A positive exact reconstructed margin proves one surface point is inside an
    intermediate box. A nonpositive/numerically failed LP is not a free-space proof.
    The LP does not use SAT axes, projection intervals, or the candidate selector.
    """
    triangle = np.asarray(triangle, float)
    start, end = np.asarray(start, float), np.asarray(end, float)
    lower, upper = np.asarray(lower, float), np.asarray(upper, float)
    if triangle.shape != (3, 3) or any(
        x.shape != (3,) for x in (start, end, lower, upper)
    ):
        raise ValueError(
            "one triangle, endpoint pair and explicit box offsets required"
        )
    if (
        not np.isfinite([*triangle, start, end, lower, upper]).all()
        or not (lower < upper).all()
    ):
        raise ValueError("finite geometry and positive box dimensions required")
    edges = (triangle[1:] - triangle[0]).T
    delta, relative = end - start, triangle[0] - start
    matrix = np.vstack(
        [
            np.column_stack([-delta, edges, np.ones(3)]),
            np.column_stack([delta, -edges, np.ones(3)]),
            [0, 1, 1, 0],
        ]
    )
    result = linprog(
        [0, 0, 0, -1],
        A_ub=matrix,
        b_ub=np.r_[upper - relative, relative - lower, 1],
        bounds=[(0, 1), (0, 1), (0, 1), (None, None)],
        method="highs-ds",
        options={
            "time_limit": 1,
            "maxiter": 1000,
            "primal_feasibility_tolerance": 1e-9,
        },
    )
    if not result.success or result.x is None or not np.isfinite(result.x).all():
        raise ValueError("surface-sweep LP unresolved: " + result.message)
    time, u, v = (Fraction(float(value)) for value in result.x[:3])
    time, u, v = (min(Fraction(1), max(Fraction(0), value)) for value in (time, u, v))
    total = u + v
    if total > 1:
        u, v = u / total, v / total
    points = [[Fraction(float(x)) for x in point] for point in triangle]
    starts, ends, lows, highs = (
        [Fraction(float(x)) for x in point] for point in (start, end, lower, upper)
    )
    point = [
        points[0][i]
        + u * (points[1][i] - points[0][i])
        + v * (points[2][i] - points[0][i])
        for i in range(3)
    ]
    foot = [a + time * (b - a) for a, b in zip(starts, ends, strict=True)]
    local = [a - b for a, b in zip(point, foot, strict=True)]
    margin = min(
        [x - lo for x, lo in zip(local, lows, strict=True)]
        + [hi - x for x, hi in zip(local, highs, strict=True)]
    )
    return {
        "lp_maximum_box_margin_m": float(result.x[3]),
        "exact_interior_witness": margin > 0,
        "reconstructed_box_margin_m": float(margin),
        "box_margin_fraction": [str(margin.numerator), str(margin.denominator)],
        "time_parameter": float(time),
        "triangle_barycentric": [float(1 - u - v), float(u), float(v)],
        "point_xyz_m": [float(x) for x in point],
        "foot_xyz_m": [float(x) for x in foot],
        "scope": "Positive reconstructed margin certifies this point against the original binary-input triangle and translated open box. It is not an exact optimum, an obstacle penetration depth, a motion plan or a free-space certificate.",
    }
