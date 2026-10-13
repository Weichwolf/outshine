"""Global cubic nearest points from endpoints and all stationary polynomial roots."""

import numpy as np
from numpy.polynomial import polynomial as poly


def segment_coefficients(controls):
    p = np.asarray(controls, dtype=float)
    return np.stack(
        (
            p[0],
            3 * (p[1] - p[0]),
            3 * (p[2] - 2 * p[1] + p[0]),
            p[3] - 3 * p[2] + 3 * p[1] - p[0],
        )
    )


def stationary_coefficients(c, points):
    relative = c[0] - points
    a = np.empty((len(points), 6))
    a[:, 0] = relative @ c[1]
    a[:, 1] = 2 * (relative @ c[2]) + np.dot(c[1], c[1])
    a[:, 2] = 3 * (relative @ c[3]) + 3 * np.dot(c[1], c[2])
    a[:, 3] = 4 * np.dot(c[1], c[3]) + 2 * np.dot(c[2], c[2])
    a[:, 4] = 5 * np.dot(c[2], c[3])
    a[:, 5] = 3 * np.dot(c[3], c[3])
    return a


def roots_in_unit(coefficients, method="batch"):
    degree = coefficients.shape[1] - 1
    while degree > 0 and np.all(coefficients[:, degree] == 0):
        degree -= 1
    coefficients = coefficients[:, : degree + 1]
    if degree == 1:
        roots = (-coefficients[:, 0] / coefficients[:, 1])[:, None].astype(complex)
    elif method == "scalar":
        roots = np.array([poly.polyroots(c) for c in coefficients])
    else:
        template = poly.polycompanion([0.0] * degree + [1.0])
        companion = np.broadcast_to(
            template, (len(coefficients), degree, degree)
        ).copy()
        companion[:, :, -1] -= coefficients[:, :degree] / coefficients[:, degree, None]
        roots = np.linalg.eigvals(companion).astype(complex)
    valid = (
        (abs(roots.imag) <= 1e-8) & (roots.real >= -1e-12) & (roots.real <= 1 + 1e-12)
    )
    return np.where(valid, np.clip(roots.real, 0, 1), np.nan)


def closest_spine(spine, points, method="bounded", block_size=2048, with_work=False):
    points = np.asarray(points, dtype=float)
    if (
        points.ndim != 2
        or points.shape[1] != 3
        or not np.isfinite(points).all()
        or len(points) > 1_000_000
    ):
        raise ValueError("bounded finite three-dimensional projection points required")
    if (
        method not in ("bounded", "batch", "scalar")
        or type(block_size) is not int
        or not 1 <= block_size <= 4096
    ):
        raise ValueError("declared polynomial method and bounded block size required")
    output = np.zeros(len(points))
    distance = np.full(len(points), np.inf)
    work = {
        "stationary_point_queries": 0,
        "point_segment_candidates": len(points) * len(spine.controls),
    }
    if method == "bounded" and len(points):
        parameter = np.linspace(0, 1, len(spine.controls) + 1)
        centres = spine.evaluate(parameter)
        for start in range(0, len(points), block_size):
            end = min(len(points), start + block_size)
            squared = np.sum((points[start:end, None] - centres[None]) ** 2, axis=-1)
            best = np.argmin(squared, axis=1)
            output[start:end] = parameter[best]
            distance[start:end] = squared[np.arange(end - start), best]
    for segment, controls in enumerate(spine.controls):
        c = segment_coefficients(controls)
        for start in range(0, len(points), block_size):
            indices = np.arange(start, min(len(points), start + block_size))
            if method == "bounded":
                box = np.asarray(controls)
                delta = np.maximum(
                    np.maximum(
                        box.min(axis=0) - points[indices],
                        points[indices] - box.max(axis=0),
                    ),
                    0,
                )
                lower = np.sum(delta**2, axis=-1)
                scale = np.maximum(
                    1,
                    np.maximum(np.max(abs(points[indices]), axis=1), np.max(abs(box))),
                )
                indices = indices[
                    lower <= distance[indices] + 64 * np.finfo(float).eps * scale**2
                ]
            if not len(indices):
                continue
            query = points[indices]
            work["stationary_point_queries"] += len(query)
            roots = roots_in_unit(
                stationary_coefficients(c, query),
                "scalar" if method == "scalar" else "batch",
            )
            parameter = np.column_stack(
                (np.zeros(len(query)), np.ones(len(query)), roots)
            )
            safe = np.nan_to_num(parameter)
            value = ((safe[..., None] * c[3] + c[2]) * safe[..., None] + c[1]) * safe[
                ..., None
            ] + c[0]
            squared = np.sum((value - query[:, None]) ** 2, axis=-1)
            squared[~np.isfinite(parameter)] = np.inf
            best = np.argmin(squared, axis=1)
            row = np.arange(len(query))
            chosen = squared[row, best] < distance[indices]
            distance[indices[chosen]] = squared[row, best][chosen]
            output[indices[chosen]] = (segment + parameter[row, best][chosen]) / len(
                spine.controls
            )
    result = (output, np.sqrt(distance))
    return (*result, work) if with_work else result
