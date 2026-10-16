"""Compare a specialized difference-constraint chain envelope with the identical LP."""

import time

import numpy as np


def validate(stations, lower, upper, rate):
    stations, lower, upper, rate = [
        np.asarray(a, float) for a in (stations, lower, upper, rate)
    ]
    n = len(stations)
    if (
        stations.shape != (n,)
        or not 2 <= n <= 256
        or lower.shape != (n,)
        or upper.shape != (n,)
        or rate.shape not in ((2,), (n - 1, 2))
        or np.any(
            np.asarray(rate).reshape(-1, 2)[:, 0]
            > np.asarray(rate).reshape(-1, 2)[:, 1]
        )
        or not all(np.isfinite(a).all() for a in (stations, lower, rate))
        or np.any(np.isnan(upper))
        or np.any(np.isneginf(upper))
        or np.any(np.diff(stations) <= 1e-10)
    ):
        raise ValueError(
            "finite bounded increasing chain and ordered derivative interval required"
        )
    return stations, lower, upper, np.broadcast_to(rate, (n - 1, 2))


def audit(stations, lower, upper, rate, result):
    if result is None or result.shape != lower.shape or not np.isfinite(result).all():
        return {"valid": False, "reason": "absent_or_nonfinite_profile"}
    delta = np.diff(result)
    ds = np.diff(stations)
    rate = np.broadcast_to(rate, (len(ds), 2))
    error = max(
        float(np.max(lower - result)),
        float(np.max(result - upper)),
        float(np.max(rate[:, 0] * ds - delta)),
        float(np.max(delta - rate[:, 1] * ds)),
    )
    return {
        "valid": error <= 1e-8,
        "reason": "" if error <= 1e-8 else "chain_bound_violation",
        "maximum_constraint_violation_m": error,
    }


def solve(stations, lower, upper, rate, *, method="chain", seconds=2):
    stations, lower, upper, rate = validate(stations, lower, upper, rate)
    if (
        method not in ("chain", "lp")
        or not np.isfinite(seconds)
        or not 0 <= seconds <= 8
    ):
        raise ValueError("known bounded bridge profile method required")
    began = time.perf_counter()
    if seconds == 0:
        return None, {"valid": False, "reason": "profile_time_budget"}
    if np.any(lower > upper):
        return None, {"valid": False, "reason": "profile_hard_bounds_conflict"}
    n, ds = len(stations), np.diff(stations)
    weights = np.r_[ds, 0] / 2 + np.r_[0, ds] / 2
    if method == "chain":
        result = lower.copy()
        for i in range(n - 1):
            result[i + 1] = max(result[i + 1], result[i] + rate[i, 0] * ds[i])
        for i in range(n - 2, -1, -1):
            result[i] = max(result[i], result[i + 1] - rate[i, 1] * ds[i])
        work = {
            "relaxations": 2 * (n - 1),
            "owned_array_bytes": result.nbytes,
            "solver": "two sweeps on a difference-constraint chain",
        }
    else:
        from scipy import sparse
        from scipy.optimize import linprog

        rows = np.repeat(np.arange(2 * (n - 1)), 2)
        columns = np.tile(np.c_[np.arange(n - 1), np.arange(1, n)].ravel(), 2)
        values = np.r_[np.tile([-1.0, 1.0], n - 1), np.tile([1.0, -1.0], n - 1)]
        matrix = sparse.csc_matrix((values, (rows, columns)), shape=(2 * (n - 1), n))
        rhs = np.r_[rate[:, 1] * ds, -rate[:, 0] * ds]
        answer = linprog(
            weights,
            A_ub=matrix,
            b_ub=rhs,
            bounds=list(zip(lower, upper, strict=True)),
            method="highs",
            options={"time_limit": seconds, "primal_feasibility_tolerance": 1e-9},
        )
        result = answer.x
        work = {
            "solver_status": answer.message,
            "iterations": answer.nit,
            "sparse_nonzeros": matrix.nnz,
            "sparse_storage_bytes": matrix.data.nbytes
            + matrix.indices.nbytes
            + matrix.indptr.nbytes,
            "solver": "HiGHS linprog",
        }
    proof = audit(stations, lower, upper, rate, result)
    elapsed = time.perf_counter() - began
    if elapsed > seconds:
        proof.update(valid=False, reason="profile_time_budget")
    if proof["valid"]:
        proof["integrated_relative_height_m2"] = float(weights @ result)
    return (result if proof["valid"] else None), dict(
        proof, method=method, seconds=elapsed, **work
    )
