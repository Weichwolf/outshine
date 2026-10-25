"""Optional existing SciPy/HiGHS challenger for the two-variable palette LP."""

import warnings

import numpy as np


def highs_pair(delta, target):
    from scipy.optimize import OptimizeWarning, linprog

    matrix = np.c_[np.r_[delta, -delta], -np.ones(2 * len(delta))]
    rhs = np.r_[target, -target]
    with warnings.catch_warnings(record=True) as notices:
        warnings.simplefilter("always", OptimizeWarning)
        result = linprog(
            [0, 1],
            A_ub=matrix,
            b_ub=rhs,
            bounds=[(0, 1), (0, None)],
            method="highs",
            options={
                "threads": 1,
                "time_limit": 2,
                "primal_feasibility_tolerance": 1e-9,
                "dual_feasibility_tolerance": 1e-9,
            },
        )
    for notice in notices:
        if (
            notice.category is not OptimizeWarning
            or str(notice.message)
            != "Unrecognized options detected: {'threads': 1}. These will be passed to HiGHS verbatim."
        ):
            raise ValueError("unexpected solver warning: " + str(notice.message))
    if not result.success or result.x is None:
        raise ValueError("bounded HiGHS pair failed: " + result.message)
    weight = float(np.clip(result.x[0], 0, 1))
    return weight, {
        "iterations": int(result.nit),
        "feasibility_rows": 2 * len(delta),
        "known_highs_input_array_bytes": matrix.nbytes + rhs.nbytes,
        "forwarded_threads_warnings": len(notices),
        "bound_adjustment": abs(weight - float(result.x[0])),
    }
