"""Bounded geometric tropism preparation and an analytic constant-direction solution."""

import numpy as np
from scipy.integrate import solve_ivp

from .quadrature import quadrature
from .scalar_forces import compile_fields, scalar_force
from .spine_curve import CubicSpine


class EvaluationBudget(ValueError):
    pass


def constant_samples(vector, initial, distances):
    rate = np.linalg.norm(vector)
    if rate == 0:
        return distances[:, None] * initial, np.broadcast_to(
            initial, (len(distances), 3)
        ).copy()
    direction = vector / rate
    cosine = np.clip(np.dot(initial, direction), -1, 1)
    transverse = initial - cosine * direction
    sine = np.linalg.norm(transverse)
    if sine == 0:
        return distances[:, None] * initial, np.broadcast_to(
            initial, (len(distances), 3)
        ).copy()
    transverse /= sine
    offset = np.arcsinh(cosine / sine)

    def tangent(s):
        angle = offset + rate * s
        log_cosh = np.logaddexp(angle, -angle) - np.log(2)
        return (
            np.tanh(angle)[..., None] * direction
            + np.exp(-log_cosh)[..., None] * transverse
        )

    x, w = quadrature(16 if rate * float(distances.max()) <= 2 else 96)
    nodes = distances[:, None] * (x + 1) / 2
    positions = (
        distances[:, None] * np.sum(tangent(nodes) * w[None, :, None], axis=1) / 2
    )
    positions[0] = 0
    tangents = tangent(distances)
    tangents[0] = initial
    return positions, tangents


def integrate(program, distances, method, max_evaluations):
    evaluations = 0
    cells = max(
        (
            len(f.rate.table) - 1 if np.any(np.diff(f.rate.table)) else 1
            for f in program.fields
        ),
        default=1,
    )
    steps = max(8, 2 * cells)
    if max_evaluations < steps * (12 if method == "DOP853" else 6):
        return None
    fields = compile_fields(program.fields)

    def rhs(distance, state):
        nonlocal evaluations
        if evaluations >= max_evaluations:
            raise EvaluationBudget("growth solver evaluation budget reached")
        evaluations += 1
        x, y, z, tx, ty, tz = (float(v) for v in state)
        fx, fy, fz = scalar_force(fields, x, y, z, distance / program.length)
        dot = fx * tx + fy * ty + fz * tz
        return np.array((tx, ty, tz, fx - dot * tx, fy - dot * ty, fz - dot * tz))

    try:
        solution = solve_ivp(
            rhs,
            (0, program.length),
            np.r_[np.zeros(3), program.initial_tangent],
            method=method,
            t_eval=distances,
            rtol=1e-10,
            atol=1e-12,
            max_step=program.length / steps,
        )
    except EvaluationBudget:
        return None
    if not solution.success or not np.isfinite(solution.y).all():
        raise ValueError("growth integration failed to produce a finite whole axis")
    return solution.y[:3].T, solution.y[3:].T, evaluations


def prepared_curve(length, positions, tangents):
    norm = np.linalg.norm(tangents, axis=1)
    error = float(abs(norm - 1).max())
    if error > 5e-8:
        raise ValueError("growth integration does not resolve the unit tangent")
    tangents = tangents / norm[:, None]
    spine = CubicSpine.from_hermite(positions[::4], length * tangents[::4])
    parameters = np.linspace(0, 1, len(positions))
    position_error = float(
        np.linalg.norm(spine.evaluate(parameters) - positions, axis=1).max()
    )
    velocity = spine.evaluate(parameters, 1)
    direction = velocity / np.linalg.norm(velocity, axis=1, keepdims=True)
    difference = np.linalg.norm(direction - tangents, axis=1)
    angle_error = float((2 * np.arcsin(np.clip(difference / 2, 0, 1))).max())
    if position_error > 5e-6 * length or angle_error > 2e-4:
        return None
    return spine, error, position_error, angle_error
