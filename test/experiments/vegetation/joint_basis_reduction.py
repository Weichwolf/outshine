"""Finite joint-response diagnostics; no runtime representation is admitted here."""

import time

import numpy as np
from positive_light_basis import angular_weights, positive_columns


def errors(predicted, reference):
    difference = predicted - reference
    return {
        "max_abs": float(abs(difference).max()),
        "rmse": float(np.sqrt(np.mean(difference**2))),
        "maximum_overestimate": float(difference.max()),
        "negative_values": int((predicted < 0).sum()),
    }


def interpolate(basis, coefficients, cones, pixels):
    return np.concatenate(
        [
            basis[v * pixels : (v + 1) * pixels] @ (coefficients @ weights)
            for v, weights in enumerate(cones)
        ]
    )


def fit_report(basis, coefficients, matrix, truth, cones, mass):
    predicted = interpolate(basis, coefficients, cones, len(mass) // len(cones))
    bounded = np.clip(predicted, 0, mass)
    report = {
        "heldout_raw": errors(predicted, truth),
        "heldout": errors(bounded, truth),
        "train": errors(np.clip(basis @ coefficients, 0, mass), matrix),
        "map_float32_bytes": basis.size * 4,
        "angular_float32_bytes": coefficients.size * 4,
        "rounding": {},
    }
    for dtype in (np.float16, np.float32):
        packed = interpolate(
            basis.astype(dtype).astype(float),
            coefficients.astype(dtype).astype(float),
            cones,
            len(mass) // len(cones),
        )
        report["rounding"][np.dtype(dtype).name] = {
            "max_delta": float(abs(np.clip(packed, 0, mass) - bounded).max()),
            "heldout": errors(np.clip(packed, 0, mass), truth),
            "payload_bytes": (basis.size + coefficients.size)
            * np.dtype(dtype).itemsize,
        }
    return bounded, report


def reduce_fields(joint, coverage, knots, held, output):
    pixels = joint.shape[-1]
    matrix = joint[:, : len(knots)].transpose(0, 2, 1).reshape(-1, len(knots))
    truth = joint[:, len(knots) :].transpose(0, 2, 1).reshape(-1, held.shape[1])
    cones = [angular_weights(knots, queries) for queries in held]
    full = np.concatenate(
        [matrix[v * pixels : (v + 1) * pixels] @ w for v, w in enumerate(cones)]
    )
    mass = coverage.reshape(-1, 1)
    arrays = {
        "truth": truth,
        "angular_raw": full,
        "angular_capped": np.clip(full, 0, mass),
    }
    report = {
        "angular": errors(arrays["angular_capped"], truth),
        "positive": {},
        "signed_svd": {},
    }
    for rank in (1, 2, 4, 8):
        began = time.perf_counter()
        fit = positive_columns(matrix, rank)
        bounded, row = fit_report(
            fit["basis"], fit["weights"], matrix, truth, cones, mass
        )
        row.update(
            selected_lights=fit["indices"].tolist(), fit_s=time.perf_counter() - began
        )
        arrays[f"positive{rank}"] = bounded
        arrays[f"positive_basis{rank}"] = fit["basis"]
        arrays[f"positive_coeff{rank}"] = fit["weights"]
        report["positive"][str(rank)] = row
    began = time.perf_counter()
    u, s, vh = np.linalg.svd(matrix, full_matrices=False)
    report["svd_fit_s"] = time.perf_counter() - began
    report["singular_values"] = s.tolist()
    for rank in (1, 2, 4, 8, 12):
        basis, coefficients = u[:, :rank] * s[:rank], vh[:rank]
        bounded, row = fit_report(basis, coefficients, matrix, truth, cones, mass)
        row["train_raw"] = errors(basis @ coefficients, matrix)
        arrays[f"signed{rank}"] = bounded
        arrays[f"signed_basis{rank}"] = basis
        arrays[f"signed_coeff{rank}"] = coefficients
        report["signed_svd"][str(rank)] = row
    np.savez_compressed(output / "reduction.npz", **arrays)
    return report
