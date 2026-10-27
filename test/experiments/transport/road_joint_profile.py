"""Bounded coupled cubic height/crossfall fallback, preserving outer profiles (#1539)."""

import numpy as np
from road_bank_profile import (
    RESIDUAL_LIMIT,
    contacts,
    derivative_matrix,
    plan_bank,
    preflight,
    prior_controls,
    residual,
)
from road_shared_profile import ProfileRecipe, shared_candidate
from scipy import sparse
from scipy.optimize import linprog


def joint_residual(curve, limits, height_change, height, bank):
    prior = curve.height
    rates = 3 * np.diff(height, axis=1) / curve.lengths[:, None]
    old_rates = 3 * np.diff(prior, axis=1) / curve.lengths[:, None]
    outer = [0, 1, prior.size - 2, prior.size - 1]
    # Unchanged input height retains its existing continuity contract. Changing
    # its controls requires a fresh C0/C1 check in actual metres/grades.
    continuity = (
        0.0
        if np.array_equal(height, prior)
        else max(
            np.max(abs(height[:-1, -1] - height[1:, 0]), initial=0),
            np.max(abs(rates[:-1, -1] - rates[1:, 0]), initial=0),
        )
    )
    return float(
        max(
            residual(
                curve,
                limits,
                prior_controls(curve),
                bank,
                height_controls=height,
                grade_mode="quadratic",
            ),
            np.max(abs(height - prior) - height_change, initial=0),
            continuity,
            abs(rates[0, 0] - old_rates[0, 0]),
            abs(rates[-1, -1] - old_rates[-1, -1]),
            np.max(abs(height.reshape(-1)[outer] - prior.reshape(-1)[outer])),
        )
    )


def height_prior_lower_bound(curve, limits):
    station = curve.stations - curve.stations[0]
    height = np.r_[curve.height[:, 0], curve.height[-1, -1]]
    lower = np.maximum(
        height[0] - limits.grade * station,
        height[-1] - limits.grade * (station[-1] - station),
    )
    upper = np.minimum(
        height[0] + limits.grade * station,
        height[-1] + limits.grade * (station[-1] - station),
    )
    return float(max(0, np.max(lower - height), np.max(height - upper)))


def program(curve, limits, height_change, bank_prior):
    n = curve.height.size
    derivative = derivative_matrix(curve.lengths)
    k0 = curve.curvature_start
    k1 = k0 + curve.curvature_rate * curve.lengths
    k = np.c_[k0, (k0 + k1) / 2, k1].reshape(-1)
    height_rate = derivative @ curve.height.reshape(-1)
    bank_rate = derivative @ bank_prior.reshape(-1)
    matrices, bounds = [], []
    for u in (-curve.width, curve.width):
        for sign in (-1, 1):
            matrices.append(
                sparse.hstack(
                    (
                        sign * derivative,
                        -sign * u * derivative,
                        sparse.csr_matrix((derivative.shape[0], 1)),
                    )
                )
            )
            bounds.append(
                limits.grade * (1 - u * k) - sign * (height_rate - u * bank_rate)
            )
    identity = sparse.eye(2 * n, format="csr")
    caps = np.r_[np.full(n, height_change), np.full(n, limits.change)]
    matrices.extend(
        (
            sparse.hstack((identity, -caps[:, None])),
            sparse.hstack((-identity, -caps[:, None])),
        )
    )
    bounds.extend((np.zeros(2 * n), np.zeros(2 * n)))
    equality, height_targets = contacts(curve, curve.height)
    _, bank_targets = contacts(curve, bank_prior)
    fixed = sparse.hstack(
        (
            sparse.block_diag((equality, equality)),
            sparse.csr_matrix((2 * len(height_targets), 1)),
        ),
        format="csc",
    )
    fixed_target = np.r_[
        height_targets - equality @ curve.height.reshape(-1),
        bank_targets - equality @ bank_prior.reshape(-1),
    ]
    variable_bounds = [(-height_change, height_change)] * n
    variable_bounds += [
        (
            max(-limits.change, -limits.crossfall - q),
            min(limits.change, limits.crossfall - q),
        )
        for q in bank_prior.reshape(-1)
    ]
    variable_bounds.append((0, 1))
    return (
        sparse.vstack(matrices, format="csc"),
        np.concatenate(bounds),
        fixed,
        fixed_target,
        variable_bounds,
    )


def plan_joint(curve, limits, height_change, *, method="staged"):
    return _plan(curve, limits, height_change, method=method, shared=False)


def plan_profile(curve, limits, height_change):
    value, note = _plan(curve, limits, height_change, method="staged", shared=True)
    if value is None:
        return None, note
    basis = "shared-cubic" if note["selected"] == "shared_cubic" else "source-cubics"
    return ProfileRecipe(basis, *value), note


def _plan(curve, limits, height_change, *, method, shared):
    if (
        method not in ("staged", "coupled")
        or not np.isfinite(height_change)
        or not 0 <= height_change <= 1000
    ):
        raise ValueError(
            "Known method and explicit finite height allowance in metres required"
        )
    base_calls = 0
    base_note = None
    if method == "staged":
        bank, bank_note = plan_bank(curve, limits)
        base_note = bank_note
        base_calls = bank_note["lp_calls"]
        if bank is not None:
            return (curve.height.copy(), bank), {
                "status": "ready",
                "selected": "bank_only",
                "base": bank_note,
                "coupled_lp_calls": 0,
                "base_lp_calls": base_calls,
            }
    reason = preflight(curve, limits, fixed_height=False)
    note = {
        "status": "pending",
        "reason": reason,
        "coupled_lp_calls": 0,
        "base_lp_calls": base_calls,
    }
    if base_note is not None:
        note["base"] = base_note
    if reason:
        return None, note
    prior_bank = prior_controls(curve)
    if np.any(abs(prior_bank) > limits.crossfall + limits.change):
        return None, note | {"reason": "coupled_prior_cap_infeasible"}
    error = joint_residual(curve, limits, height_change, curve.height, prior_bank)
    if method == "staged" and error <= RESIDUAL_LIMIT:
        return (curve.height.copy(), prior_bank), note | {
            "status": "ready",
            "selected": "unchanged_quadratic",
        }
    necessary = height_prior_lower_bound(curve, limits)
    note["height_change_lower_bound_m"] = necessary
    if necessary > height_change:
        return None, note | {"reason": "height_prior_budget"}
    # A single cubic's four controls are entirely fixed by the two end profiles.
    if len(curve.lengths) == 1:
        error = joint_residual(curve, limits, height_change, curve.height, prior_bank)
        if error <= RESIDUAL_LIMIT:
            return (curve.height.copy(), prior_bank), note | {
                "status": "ready",
                "selected": "fixed_cubic",
            }
        return None, note | {
            "reason": "fixed_cubic_family",
            "maximum_numeric_residual": error,
        }
    if shared:
        candidate, certificate = shared_candidate(curve, limits, height_change)
        note["shared_candidate"] = certificate
        if candidate is not None:
            return candidate, note | {
                "status": "ready",
                "reason": "",
                "selected": "shared_cubic",
            }
    return fit_local(curve, limits, height_change, prior_bank, note)


def fit_local(curve, limits, height_change, prior_bank, note):
    matrix, bound, fixed, targets, variable_bounds = program(
        curve, limits, height_change, prior_bank
    )
    n = curve.height.size
    note.update(
        variables=2 * n + 1,
        inequalities=matrix.shape[0],
        nonzeros=matrix.nnz + fixed.nnz,
    )
    solved = linprog(
        np.r_[np.zeros(2 * n), 1.0],
        A_ub=matrix,
        b_ub=bound,
        A_eq=fixed,
        b_eq=targets,
        bounds=variable_bounds,
        method="highs",
        options={"time_limit": limits.seconds, "maxiter": limits.max_iterations},
    )
    note.update(
        coupled_lp_calls=1,
        solver_status=int(solved.status),
        solver_iterations=int(solved.nit or 0),
    )
    if not solved.success:
        return None, note | {
            "reason": "coupled_program_infeasible"
            if solved.status == 2
            else "coupled_incomplete"
        }
    height = curve.height + solved.x[:n].reshape(curve.height.shape)
    bank = prior_bank + solved.x[n : 2 * n].reshape(prior_bank.shape)
    outer = [0, 1, n - 2, n - 1]
    height.reshape(-1)[outer] = curve.height.reshape(-1)[outer]
    bank.reshape(-1)[outer] = prior_bank.reshape(-1)[outer]
    if not np.isfinite(height).all() or not np.isfinite(bank).all():
        return None, note | {"reason": "coupled_nonfinite"}
    error = joint_residual(curve, limits, height_change, height, bank)
    if error > RESIDUAL_LIMIT:
        return None, note | {
            "reason": "coupled_residual",
            "maximum_numeric_residual": error,
        }
    return (height, bank), note | {
        "status": "ready",
        "reason": "",
        "selected": "coupled",
        "maximum_numeric_residual": error,
        "maximum_height_control_change_m": float(abs(height - curve.height).max()),
        "maximum_bank_control_change": float(abs(bank - prior_bank).max()),
    }
