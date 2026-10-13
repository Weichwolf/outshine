"""Bounded surface gradients and level constraints on a regular height lattice."""

import numpy as np
import time


def gradient(height, spacing):
    a, b = height[:, :-1, :-1], height[:, :-1, 1:]
    c, d = height[:, 1:, :-1], height[:, 1:, 1:]
    return np.stack([np.stack([b-a, c-a], axis=-1),
                     np.stack([d-c, d-b], axis=-1)], axis=-2) / spacing


def adjoint(value, spacing, shape):
    result = np.zeros(shape)
    p, q = value[..., 0, :], value[..., 1, :]
    result[:, :-1, :-1] -= p[..., 0] + p[..., 1]
    result[:, :-1, 1:] += p[..., 0] - q[..., 1]
    result[:, 1:, :-1] += p[..., 1] - q[..., 0]
    result[:, 1:, 1:] += q[..., 0] + q[..., 1]
    return result / spacing


def solve(target, spacing, maximum, lower, upper, gaps, equal, iterations=5000, tolerance=2e-5):
    height, extrapolated = target.copy(), target.copy()
    dual = np.zeros((*target.shape[:1], target.shape[1]-1, target.shape[2]-1, 2, 2))
    contact = np.zeros(len(lower))
    degree = np.bincount(np.r_[lower, upper], minlength=target.size)
    primal_step = .95 / (8 + degree.max(initial=0))
    dual_step = .95 / 2
    began = time.perf_counter()
    report = dict(complete=False, iterations=0)
    for iteration in range(1, iterations + 1):
        dual += dual_step * gradient(extrapolated, 1)
        length = np.linalg.norm(dual, axis=-1)
        scale = np.maximum(0., 1-dual_step * maximum * spacing / np.maximum(length, 1e-30))
        dual *= scale[..., None]
        flat = extrapolated.ravel()
        contact += dual_step * (flat[upper] - flat[lower])
        contact[~equal] = np.minimum(0., contact[~equal] - dual_step*gaps[~equal])
        derivative = adjoint(dual, 1, target.shape).ravel()
        np.add.at(derivative, lower, -contact)
        np.add.at(derivative, upper, contact)
        fitted = (height - primal_step*derivative.reshape(target.shape) + primal_step*target) / (1+primal_step)
        theta = 1 / np.sqrt(1 + 2*primal_step)
        extrapolated = fitted + theta*(fitted-height)
        height = fitted
        primal_step *= theta
        dual_step /= theta
        if iteration % 100 == 0:
            actual = np.linalg.norm(gradient(height, spacing), axis=-1)
            grade_error = float(np.maximum(actual-maximum, 0).max(initial=0))
            rise = height.ravel()[upper] - height.ravel()[lower]
            gap_error = float(np.maximum(gaps[~equal]-rise[~equal], 0).max(initial=0))
            equality_error = float(abs(rise[equal]).max(initial=0))
            primal_value = .5 * float(np.sum((height-target)**2))
            finite = np.isfinite(maximum)
            support = float(np.sum(maximum[finite]*spacing*np.linalg.norm(dual, axis=-1)[finite]))
            dual_value = (float(target.ravel() @ derivative) - .5*float(derivative @ derivative)
                          - support - float(gaps[~equal] @ contact[~equal]))
            objective_gap = primal_value - dual_value
            report.update(iterations=iteration, maximum_grade_error=grade_error,
                          maximum_clearance_error_m=gap_error, maximum_binding_error_m=equality_error,
                          objective=primal_value, dual_lower_bound=dual_value, objective_gap=objective_gap)
            if (max(grade_error, gap_error, equality_error) <= tolerance
                    and -1e-6 <= objective_gap <= max(1e-6, primal_value*1e-5)):
                report['complete'] = True
                break
    report.update(solve_ms=(time.perf_counter()-began)*1000,
                  maximum_height_change_m=float(abs(height-target).max()),
                  rms_height_change_m=float(np.sqrt(np.mean((height-target)**2))))
    return height, report


def verify():
    rng = np.random.default_rng(1)
    height = rng.normal(size=(2, 9, 9))
    value = rng.normal(size=(2, 8, 8, 2, 2))
    assert abs(np.sum(gradient(height, 10)*value) - np.sum(height*adjoint(value, 10, height.shape))) < 1e-10
    target = np.zeros((2, 13, 13))
    target[1] = 6
    limit = np.full((2, 12, 12, 2), .08)
    point = np.arange(13*13).reshape(13,13)
    lower = np.r_[point[:, 0], point[4:9,4:9].ravel()]
    upper = lower + 13*13
    equality = np.r_[np.ones(13,dtype=bool), np.zeros(25,dtype=bool)]
    gaps = np.where(equality, 0, 5.75)
    _, report = solve(target, 10, limit, lower, upper, gaps, equality)
    return report


if __name__ == '__main__':
    print(verify())
