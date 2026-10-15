"""Whole-face constraints and contraction to the declared horizontal reference."""

import numpy as np


def gradients(coefficients, faces, heights):
    values = heights[faces]
    return np.einsum("fij,fj->fi", coefficients, values - values[:, :1])


def inspect(coefficients, faces, frames, heights, fixed, datum, target, weights):
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        grade = gradients(coefficients, faces, heights)
        maximum = float(
            np.linalg.norm(np.einsum("fij,fj->fi", frames, grade), axis=1).max()
        )
        error = abs(heights - target)
        return {
            "maximum_normalized_grade": maximum,
            "maximum_pin_error_m": float(abs(heights[fixed] - datum).max(initial=0)),
            "maximum_dem_vertex_error_m": float(error.max()),
            "weighted_dem_rms_m": float(np.sqrt(np.average(error**2, weights=weights))),
            "dem_vertex_rms_m": float(np.sqrt(np.mean(error**2))),
            "dem_vertex_p95_m": float(np.percentile(error, 95)),
        }


def conservative_candidate(heights, coefficients, faces, frames, fixed, datum):
    z = np.array(heights, dtype=float, copy=True)
    if z.ndim != 1 or not np.isfinite(z).all():
        return None, {"reason": "nonfinite_or_incomplete_solver_candidate"}
    pin_change = float(abs(z[fixed] - datum).max(initial=0))
    z[fixed] = datum
    with np.errstate(over="ignore", invalid="ignore"):
        grade = gradients(coefficients, faces, z)
        maximum = float(
            np.linalg.norm(np.einsum("fij,fj->fi", frames, grade), axis=1).max()
        )
    if not np.isfinite(maximum):
        return None, {"reason": "nonfinite_solver_gradient"}
    factor = min(1.0, (1 - 1e-5) / max(maximum, 1e-30))
    return datum + factor * (z - datum), {
        "feasible_flat_contraction": factor,
        "maximum_pin_projection_m": pin_change,
        "before_contraction_maximum_normalized_grade": maximum,
        "reason": "",
        "scope": "Homogeneous-gradient contraction toward the explicit flat datum; contacts projected first. Whole-face tolerance is unchanged; no optimality claim.",
    }
