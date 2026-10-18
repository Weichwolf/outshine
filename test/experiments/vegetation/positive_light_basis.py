"""Bounded positive angular interpolation and greedy actual-column NNLS reduction."""

import numpy as np
from scipy.optimize import nnls
from scipy.spatial import ConvexHull, QhullError


def unit_queries(values, maximum):
    values = np.asarray(values, float)
    if (
        values.ndim != 2
        or values.shape[1:] != (3,)
        or not 1 <= len(values) <= maximum
        or not np.isfinite(values).all()
        or np.any(abs(np.linalg.norm(values, axis=1) - 1) > 1e-10)
    ):
        raise ValueError("bounded finite unit directions required")
    return values


def angular_weights(knots, queries):
    knots, queries = unit_queries(knots, 128), unit_queries(queries, 128)
    if len(knots) < 6:
        raise ValueError("at least six enclosing sphere knots required")
    try:
        hull = ConvexHull(knots)
    except QhullError as error:
        raise ValueError("light knots must span an enclosing sphere") from error
    if np.max(hull.equations[:, -1]) >= -1e-10:
        raise ValueError("light knots must strictly enclose the origin")
    faces = np.sort(hull.simplices, axis=1)
    faces = faces[np.lexsort(faces.T[::-1])]
    inverse = np.linalg.inv(knots[faces].transpose(0, 2, 1))
    candidates = np.einsum("fij,qj->qfi", inverse, queries)
    inside = candidates.min(axis=-1) >= -1e-12
    if not inside.any(axis=1).all():
        raise ValueError("query cone not found")
    chosen = np.argmax(inside, axis=1)
    result = np.zeros((len(knots), len(queries)))
    for row, face in enumerate(chosen):
        result[faces[face], row] = np.maximum(candidates[row, face], 0)
    return result


def positive_columns(matrix, max_rank=8):
    matrix = np.asarray(matrix, float)
    if (
        matrix.ndim != 2
        or not 1 <= matrix.shape[0] <= 4096
        or not 1 <= matrix.shape[1] <= 128
        or not np.isfinite(matrix).all()
        or np.any(matrix < 0)
        or matrix.max(initial=0) > 2_000_000
        or type(max_rank) is not int
        or not 1 <= max_rank <= min(8, matrix.shape[1])
    ):
        raise ValueError("bounded nonnegative response matrix and rank1..8 required")
    indices = []
    predicted = np.zeros_like(matrix)
    steps = []
    for _ in range(max_rank):
        residual = matrix - predicted
        if abs(residual).max(initial=0) <= 1e-12:
            break
        error = np.sum(residual * residual, axis=0)
        error[indices] = -1
        indices.append(int(np.argmax(error)))
        basis = matrix[:, indices].copy()
        weights = np.column_stack(
            [nnls(basis, column, maxiter=6 * len(indices))[0] for column in matrix.T]
        )
        predicted = basis @ weights
        steps.append(
            {
                "rank": len(indices),
                "max_abs_error": float(abs(predicted - matrix).max()),
                "rmse": float(np.sqrt(np.mean((predicted - matrix) ** 2))),
            }
        )
    if not indices:
        basis = np.empty((len(matrix), 0))
        weights = np.empty((0, matrix.shape[1]))
    return {
        "indices": np.array(indices, int),
        "basis": basis,
        "weights": weights,
        "steps": steps,
    }
