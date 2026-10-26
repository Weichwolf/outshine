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
        "known_solver_expression_arrays": 3 * (count + 1),
        "pair_dispatch_rounds": count,
    }


def interval_pairs(delta, target, *, iterations=48):
    """Same pair bisections in contiguous rows, without reevaluating stopped rows."""
    delta, target = np.asarray(delta, float), np.asarray(target, float)
    if (
        delta.ndim != 2
        or target.shape != delta.shape
        or min(delta.shape) == 0
        or not np.isfinite(delta).all()
        or not np.isfinite(target).all()
    ):
        raise ValueError("finite nonempty paired affine rows required")
    count, width = delta.shape
    input_bytes = delta.nbytes + target.nbytes
    lower, upper = np.zeros(count), np.ones(count)
    first, second = abs(target).max(axis=1), abs(delta - target).max(axis=1)
    best, loss = np.where(first <= second, 0.0, 1.0), np.minimum(first, second)
    owners = np.arange(count)
    rounds = steps = compact_bytes = compact_arrays = 0
    expression_bytes = 3 * delta.nbytes
    for _ in range(iterations):
        weight = (lower + upper) / 2
        residual = weight[:, None] * delta - target
        row = np.argmax(abs(residual), axis=1)
        local = np.arange(len(owners))
        value = residual[local, row]
        error = abs(value)
        improved = error < loss[owners]
        selected = owners[improved]
        best[selected], loss[selected] = weight[improved], error[improved]
        slope = np.sign(value) * delta[local, row]
        steps += len(owners)
        expression_bytes += 3 * delta.nbytes
        rounds += 1
        upper[slope > 0] = weight[slope > 0]
        lower[slope < 0] = weight[slope < 0]
        active = slope != 0
        if not active.any():
            break
        if not active.all():
            delta, target = delta[active], target[active]
            lower, upper, owners = lower[active], upper[active], owners[active]
            compact_bytes += delta.nbytes + target.nbytes
            compact_arrays += 2
    return best, {
        "iterations": steps,
        "objective_rows": width * steps,
        "evaluated_pair_steps": steps,
        "known_solver_expression_array_bytes": expression_bytes,
        "known_solver_expression_arrays": 3 * (rounds + 1),
        "known_compacted_pair_array_bytes": compact_bytes,
        "known_compacted_pair_arrays": compact_arrays,
        "pair_dispatch_rounds": rounds,
        "pair_input_array_bytes": input_bytes,
    }


def solve_palette_pairs(spectrum, target, pairs, pair_solver):
    if pair_solver is interval_pair:
        left, right = np.asarray(pairs).T
        delta = (spectrum[:, left] - spectrum[:, right]).T
        wanted = (target[:, None] - spectrum[:, right]).T
        delta_rows, target_rows = map(np.ascontiguousarray, (delta, wanted))
        weights, work = interval_pairs(delta_rows, target_rows)
        copied = (
            int(not np.shares_memory(delta, delta_rows))
            + int(not np.shares_memory(wanted, target_rows))
        ) * delta.nbytes
        work["known_pair_setup_array_bytes"] = 5 * delta.nbytes + copied
        work["known_pair_setup_arrays"] = 5 + copied // delta.nbytes
        work["known_contiguous_pair_copy_bytes"] = copied
        return weights, work
    weights, work = [], {}
    for a, b in pairs:
        weight, counters = pair_solver(
            spectrum[:, a] - spectrum[:, b], target - spectrum[:, b]
        )
        weights.append(weight)
        for key, value in counters.items():
            work[key] = work.get(key, 0) + value
    work["known_pair_setup_array_bytes"] = 2 * len(pairs) * target.nbytes
    work["known_pair_setup_arrays"] = 2 * len(pairs)
    work["pair_input_array_bytes"] = 2 * target.nbytes
    return weights, work


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
        pairs = list(itertools.combinations(range(len(original)), 2))
        weights, counters = solve_palette_pairs(spectrum, target, pairs, pair_solver)
        work |= counters | {"pairs": len(pairs)}
        for (a, b), weight in zip(pairs, weights, strict=True):
            weight = float(weight)
            fitted = np.zeros_like(original)
            fitted[a], fitted[b] = weight * mass, (1 - weight) * mass
            error = float(abs(spectrum @ (fitted.astype(float) / mass) - target).max())
            if error < loss:
                best, loss = fitted, error
    return best, work | {
        "training_relative_error": loss,
        "already_within_budget": False,
    }
