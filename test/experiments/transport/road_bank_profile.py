"""Bounded C1 crossfall recipes with fixed outer profiles (#1533)."""

from dataclasses import dataclass

import numpy as np
from road_surface_model import controls
from scipy import sparse
from scipy.optimize import linprog

RESIDUAL_LIMIT = 1e-9


@dataclass(frozen=True)
class BankLimits:
    grade: float
    crossfall: float
    change: float
    max_spans: int = 512
    max_iterations: int = 20000
    seconds: float = 1.0

    def __post_init__(self):
        if (
            not np.isfinite(
                [self.grade, self.crossfall, self.change, self.seconds]
            ).all()
            or not 0 < self.grade <= 1
            or not 0 < self.crossfall <= 1
            or not 0 <= self.change <= 1
            or not 0 < self.seconds <= 60
            or type(self.max_spans) is not int
            or not 1 <= self.max_spans <= 4096
            or type(self.max_iterations) is not int
            or not 1 <= self.max_iterations <= 1_000_000
        ):
            raise ValueError(
                "Explicit finite grade/crossfall/change and bounded work required"
            )


def prior_controls(curve):
    q = np.tan(curve.poses[:, 9])
    rate = curve.poses[:, 10] / np.cos(curve.poses[:, 9]) ** 2
    return controls(q[:-1], q[1:], rate[:-1], rate[1:], curve.lengths).T


def preflight(curve, limits, *, fixed_height=True):
    if len(curve.lengths) > limits.max_spans:
        return "source_span_budget"
    if np.any(curve.width * curve.curvature_max >= 1):
        return "folded_width"
    q = np.tan(curve.poses[[0, -1], 9])
    if np.any(abs(q) > limits.crossfall):
        return "fixed_crossfall"
    grade = 3 * np.diff(curve.height, axis=1) / curve.lengths[:, None]
    a, b, c = grade.T
    denominator = a - 2 * b + c
    t = np.clip(
        np.divide(a - b, denominator, out=np.zeros_like(a), where=denominator != 0),
        0,
        1,
    )
    peak = np.maximum(
        np.maximum(abs(a), abs(c)),
        abs(a * (1 - t) ** 2 + 2 * b * t * (1 - t) + c * t * t),
    )
    if fixed_height and np.any(peak > limits.grade):
        return "fixed_center_grade"
    rate = curve.poses[[0, -1], 10] / np.cos(curve.poses[[0, -1], 9]) ** 2
    curvature = [
        curve.curvature_start[0],
        curve.curvature_start[-1] + curve.curvature_rate[-1] * curve.lengths[-1],
    ]
    for u in (-curve.width, curve.width):
        end_grades = (curve.poses[[0, -1], 7] - u * rate) / (
            1 - u * np.array(curvature)
        )
        if np.any(abs(end_grades) > limits.grade):
            return "fixed_edge_grade"
    dh = np.diff(curve.poses[[0, -1], 3])[0]
    dq = q[1] - q[0]
    integral_k = np.sum(
        (curve.curvature_start + curve.curvature_rate * curve.lengths / 2)
        * curve.lengths
    )
    for u in (-curve.width, curve.width):
        run = np.sum(curve.lengths) - u * integral_k
        if abs(dh - u * dq) > limits.grade * run:
            return "fixed_edge_displacement"
    return ""


def derivative_matrix(lengths):
    n = len(lengths)
    row = np.repeat(np.arange(3 * n), 2)
    column = np.repeat(4 * np.arange(n), 6) + np.tile([0, 1, 1, 2, 2, 3], n)
    values = np.repeat(3 / lengths, 6) * np.tile([-1, 1], 3 * n)
    return sparse.csr_matrix((values, (row, column)), shape=(3 * n, 4 * n))


def inequalities(curve, limits, prior):
    n = len(curve.lengths)
    derivative = derivative_matrix(curve.lengths)
    height = (3 * np.diff(curve.height, axis=1) / curve.lengths[:, None]).reshape(-1)
    k0 = curve.curvature_start
    k1 = k0 + curve.curvature_rate * curve.lengths
    curvature = np.c_[k0, (k0 + k1) / 2, k1].reshape(-1)
    matrix, bounds = [], []
    for u in (-curve.width, curve.width):
        allowance = limits.grade * (1 - u * curvature)
        for sign in (-1, 1):
            matrix.append(-sign * u * derivative)
            bounds.append(allowance - sign * height)
    identity = sparse.eye(4 * n, format="csr")
    matrix.extend((identity, -identity))
    bounds.extend(
        (prior.reshape(-1) + limits.change, -prior.reshape(-1) + limits.change)
    )
    return sparse.vstack(matrix, format="csr"), np.concatenate(bounds)


def contacts(curve, prior):
    n = len(curve.lengths)
    rows, cols, data, bound = [], [], [], []

    def add(columns, values, target):
        rows.extend([len(bound)] * len(columns))
        cols.extend(columns)
        data.extend(values)
        bound.append(target)

    for i in range(n - 1):
        add([4 * i + 3, 4 * i + 4], [1, -1], 0)
        left, right = curve.lengths[i : i + 2]
        # Multiply by a harmonic scale to avoid 1/L conditioning on tiny spans.
        scale = 1 / (1 / left + 1 / right)
        add(
            [4 * i + 2, 4 * i + 3, 4 * i + 4, 4 * i + 5],
            [-scale / left, scale / left, scale / right, -scale / right],
            0,
        )
    for index in (0, 1, 4 * n - 2, 4 * n - 1):
        add([index], [1], prior.reshape(-1)[index])
    return sparse.csr_matrix((data, (rows, cols)), shape=(len(bound), 4 * n)), np.array(
        bound
    )


def quadratic_minimum(values):
    a, b, c = np.asarray(values, dtype=float).T
    denominator = a - 2 * b + c
    t = np.clip(
        np.divide(a - b, denominator, out=np.zeros_like(a), where=denominator > 0), 0, 1
    )
    return np.minimum(
        np.minimum(a, c), a * (1 - t) ** 2 + 2 * b * t * (1 - t) + c * t * t
    )


def grade_residual(curve, limits, height, rates, mode):
    k0 = curve.curvature_start
    k1 = k0 + curve.curvature_rate * curve.lengths
    curvature = np.c_[k0, (k0 + k1) / 2, k1]
    errors = []
    for u in (-curve.width, curve.width):
        denominator = 1 - u * curvature
        numerator = height - u * rates
        if mode == "bernstein":
            errors.append(float(np.max(abs(numerator) / denominator - limits.grade)))
        elif mode == "quadratic":
            minimum_denominator = np.minimum(denominator[:, 0], denominator[:, -1])
            for sign in (-1, 1):
                lower = quadratic_minimum(limits.grade * denominator - sign * numerator)
                errors.append(float(np.max(-lower / minimum_denominator)))
        else:
            raise ValueError("Known whole-interval grade certificate required")
    return max(errors)


def residual(
    curve, limits, prior, candidate, *, height_controls=None, grade_mode="bernstein"
):
    x = candidate.reshape(-1)
    rates = 3 * np.diff(candidate, axis=1) / curve.lengths[:, None]
    original_rates = 3 * np.diff(prior, axis=1) / curve.lengths[:, None]
    height = curve.height if height_controls is None else height_controls
    grade = 3 * np.diff(height, axis=1) / curve.lengths[:, None]
    physical = grade_residual(curve, limits, grade, rates, grade_mode)
    return float(
        max(
            physical,
            np.max(abs(candidate - prior) - limits.change, initial=0),
            np.max(abs(candidate[:-1, -1] - candidate[1:, 0]), initial=0),
            np.max(abs(x) - limits.crossfall, initial=0),
            np.max(abs(rates[:-1, -1] - rates[1:, 0]), initial=0),
            abs(rates[0, 0] - original_rates[0, 0]),
            abs(rates[-1, -1] - original_rates[-1, -1]),
            np.max(
                abs(
                    x[[0, 1, x.size - 2, x.size - 1]]
                    - prior.reshape(-1)[[0, 1, x.size - 2, x.size - 1]]
                )
            ),
        )
    )


def outer_cubic(curve, prior):
    start, end = curve.stations[[0, -1]]
    length = end - start
    rate_start = 3 * (prior[0, 1] - prior[0, 0]) / curve.lengths[0]
    rate_end = 3 * (prior[-1, 3] - prior[-1, 2]) / curve.lengths[-1]
    return controls(prior[0, 0], prior[-1, 3], rate_start, rate_end, length)


def global_cubic(curve, prior):
    from road_surface_model import bezier

    start, end = curve.stations[[0, -1]]
    length = end - start
    control = outer_cubic(curve, prior)
    t = (curve.stations - start) / length
    q = bezier(control, t)
    rate = bezier(3 * np.diff(control) / length, t)
    return controls(q[:-1], q[1:], rate[:-1], rate[1:], curve.lengths).T


def plan_bank(curve, limits, *, method="adaptive"):
    if method not in ("adaptive", "linear", "global"):
        raise ValueError("Known bounded bank method required")
    reason = preflight(curve, limits)
    note = {"status": "pending", "reason": reason, "lp_calls": 0, "method": method}
    if reason:
        return None, note
    prior = prior_controls(curve)
    if method == "adaptive" and residual(curve, limits, prior, prior) <= RESIDUAL_LIMIT:
        note.update(
            status="ready", reason="", selected="prior", maximum_control_change=0.0
        )
        return prior, note
    if method in ("adaptive", "global"):
        candidate = global_cubic(curve, prior)
        error = residual(curve, limits, prior, candidate)
        if error <= RESIDUAL_LIMIT:
            note.update(
                status="ready",
                reason="",
                selected="global",
                maximum_control_change=float(abs(candidate - prior).max()),
            )
            return candidate, note
        if method == "global":
            note.update(reason="global_unqualified", maximum_constraint_residual=error)
            return None, note
    size = prior.size
    matrix, bounds = inequalities(curve, limits, prior)
    equality, targets = contacts(curve, prior)
    identity = sparse.eye(size, format="csr")
    program = sparse.vstack(
        (
            sparse.hstack((matrix, sparse.csr_matrix((matrix.shape[0], 1)))),
            sparse.hstack((identity, -np.ones((size, 1)))),
            sparse.hstack((-identity, -np.ones((size, 1)))),
        ),
        format="csc",
    )
    upper = np.r_[bounds, prior.reshape(-1), -prior.reshape(-1)]
    objective = np.r_[np.zeros(size), 1.0]
    fixed = sparse.hstack(
        (equality, sparse.csr_matrix((equality.shape[0], 1))), format="csc"
    )
    note.update(
        variables=size + 1,
        inequalities=program.shape[0],
        equalities=fixed.shape[0],
        nonzeros=program.nnz + fixed.nnz,
    )
    solved = linprog(
        objective,
        A_ub=program,
        b_ub=upper,
        A_eq=fixed,
        b_eq=targets,
        bounds=[(-limits.crossfall, limits.crossfall)] * size + [(0, limits.change)],
        method="highs",
        options={"time_limit": limits.seconds, "maxiter": limits.max_iterations},
    )
    note.update(
        lp_calls=1, solver_status=int(solved.status), solver_iterations=int(solved.nit)
    )
    if not solved.success:
        note.update(
            reason="linear_infeasible" if solved.status == 2 else "linear_incomplete"
        )
        return None, note
    candidate = solved.x[:-1].reshape(prior.shape)
    candidate.reshape(-1)[[0, 1, candidate.size - 2, candidate.size - 1]] = (
        prior.reshape(-1)[[0, 1, prior.size - 2, prior.size - 1]]
    )
    error = residual(curve, limits, prior, candidate)
    if not np.isfinite(candidate).all() or error > RESIDUAL_LIMIT:
        note.update(reason="linear_residual", maximum_constraint_residual=error)
        return None, note
    note.update(
        status="ready",
        reason="",
        selected="linear",
        maximum_control_change=float(abs(candidate - prior).max()),
        maximum_constraint_residual=error,
    )
    return candidate, note
