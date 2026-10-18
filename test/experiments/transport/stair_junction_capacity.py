"""Floating-point primal/dual check for this finite model's common-gap bound."""

import numpy as np


def dual_bound(matrix, bounds, rhs, objective, answer):
    lo = np.array([-np.inf if a is None else a for a, _b in bounds])
    hi = np.array([np.inf if b is None else b for _a, b in bounds])
    lower, upper = answer.lower.marginals, answer.upper.marginals
    dual = answer.ineqlin.marginals
    lhs = matrix @ answer.x
    stationarity = float(abs(objective - matrix.T @ dual - lower - upper).max())
    primal_error = float(
        max((lhs - rhs).max(), (lo - answer.x).max(), (answer.x - hi).max())
    )
    sign_error = float(max(dual.max(), (-lower).max(), upper.max()))
    dual_objective = float(
        np.array(rhs) @ dual
        + lo[np.isfinite(lo)] @ lower[np.isfinite(lo)]
        + hi[np.isfinite(hi)] @ upper[np.isfinite(hi)]
    )
    gap = float(answer.fun - dual_objective)
    if max(stationarity, primal_error, sign_error, abs(gap)) > 1e-7:
        raise ValueError("finite capacity primal/dual residual is too large")
    return {
        "primal_common_surface_gap_m": float(answer.x[-1]),
        "numerical_dual_upper_gap_m": -dual_objective,
        "stationarity_max_abs": stationarity,
        "primal_max_violation": primal_error,
        "dual_sign_max_violation": sign_error,
        "duality_gap": gap,
        "scope": "Numerical primal/dual bound for this exact finite plan, chosen landings, profiles, Float32 constraints and height budget; common gap capped at5m. No exact interval-arithmetic, arbitrary-recipe, native-operand or global-network impossibility claim.",
    }
