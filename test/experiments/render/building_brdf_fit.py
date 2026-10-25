"""Bounded positive palette fits; finite-grid response error, never mean roughness."""

import itertools

import numpy as np


def interval_pair(delta, target, *, iterations=48):
    """Bisect the convex maximum of affine residuals without reciprocal deltas."""
    lower, upper = 0.0, 1.0
    endpoint = [float(abs(target).max()), float(abs(delta - target).max())]
    best, loss = float(np.argmin(endpoint)), min(endpoint)
    count = 0
    for _ in range(iterations):
        weight = (lower + upper) / 2
        residual = weight * delta - target
        row = int(np.argmax(abs(residual)))
        value = abs(float(residual[row]))
        if value < loss:
            best, loss = weight, value
        slope = np.sign(residual[row]) * delta[row]
        count += 1
        if slope == 0:
            break
        if slope > 0:
            upper = weight
        else:
            lower = weight
    return best, {
        "iterations": count,
        "objective_rows": len(delta) * count,
        "known_solver_expression_array_bytes": 3 * delta.nbytes * (count + 1),
    }


def relative_response(weights, albedo, basis):
    weights, albedo = np.asarray(weights, float), np.asarray(albedo, float)
    spectrum, diffuse = basis["specular"], basis["diffuse"]
    if (
        weights.shape != (spectrum.shape[1],)
        or albedo.shape != (3,)
        or not np.isfinite(weights).all()
        or np.any(weights < 0)
        or not np.isfinite(albedo).all()
        or np.any((albedo < 0) | (albedo > 1))
        or abs(weights.sum() - 1) > 8 * np.finfo(np.float32).eps
    ):
        raise ValueError(
            "owned nonnegative unit material measure and physical albedo required"
        )
    mass = float(weights.sum())
    target = spectrum @ (weights / mass)
    denominator = diffuse * albedo.min() + target
    if np.any(denominator <= 0):
        raise ValueError("positive direct-response samples required")
    return spectrum / denominator[:, None], target / denominator, mass


def emitted_error(original, fitted, albedo, basis):
    original, fitted = np.asarray(original, float), np.asarray(fitted, float)
    diffuse = basis["diffuse"][:, None] * np.asarray(albedo, float)
    target = diffuse * original.sum() + (basis["specular"] @ original)[:, None]
    candidate = diffuse * fitted.sum() + (basis["specular"] @ fitted)[:, None]
    if np.any(target <= 0):
        raise ValueError("positive direct material responses required")
    return abs(candidate - target) / target


def fit_weights(weights, albedo, basis, *, max_lobes=2, pair_solver=interval_pair):
    original = np.asarray(weights)
    if max_lobes not in (1, 2):
        raise ValueError("one or two positive palette components required")
    spectrum, target, mass = relative_response(weights, albedo, basis)
    if np.count_nonzero(original) <= max_lobes:
        return original.copy(), {
            "pairs": 0,
            "iterations": 0,
            "objective_rows": 0,
            "training_relative_error": 0.0,
            "already_within_budget": True,
        }
    errors = abs(spectrum - target[:, None]).max(axis=0)
    first = int(np.argmin(errors))
    best = np.zeros_like(original)
    best[first] = mass
    loss = float(abs(spectrum @ (best.astype(float) / mass) - target).max())
    work = {"pairs": 0, "iterations": 0, "objective_rows": 0}
    if max_lobes == 2:
        for a, b in itertools.combinations(range(len(original)), 2):
            weight, counters = pair_solver(
                spectrum[:, a] - spectrum[:, b], target - spectrum[:, b]
            )
            work["pairs"] += 1
            for key, value in counters.items():
                work[key] = work.get(key, 0) + value
            fitted = np.zeros_like(original)
            fitted[a], fitted[b] = weight * mass, (1 - weight) * mass
            error = float(abs(spectrum @ (fitted.astype(float) / mass) - target).max())
            if error < loss:
                best, loss = fitted, error
    return best, work | {
        "training_relative_error": loss,
        "already_within_budget": False,
    }
