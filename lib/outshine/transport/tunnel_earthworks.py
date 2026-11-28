"""Fit a shallow fill above a road tunnel with fixed surrounding contacts."""

from dataclasses import dataclass

import numpy as np
import shapely
from scipy import sparse
from scipy.optimize import linprog

from .heights import _gradient_weights

MAX_FILL_GRADE = 0.10


@dataclass(frozen=True, slots=True)
class CoverLift:
    vertices: np.ndarray
    displacement: np.ndarray


def cover_lift_patch(vertices, faces, required, guides, frozen, width):
    first, last = np.flatnonzero(required)[[0, -1]]
    centre = shapely.LineString(guides.sections[first : last + 2].mean(axis=1))
    maximum_fill = width / 3
    reach = 3 * maximum_fill / MAX_FILL_GRADE
    supports = shapely.points(vertices[:, :2])
    delta = np.diff(vertices[faces, :2], axis=1)
    diagonal = np.linalg.norm(delta.sum(axis=1), axis=1).max()
    end_sections = shapely.linestrings(guides.sections[[0, 1, -2, -1]])
    pins = shapely.union_all([frozen, *end_sections])
    eligible = shapely.dwithin(centre, supports, reach)
    eligible &= ~shapely.dwithin(pins, supports, diagonal)
    return eligible, maximum_fill


def _lift_matrix(indices, coefficients, eligible):
    variable = np.full(len(eligible), -1, dtype=int)
    variable[eligible] = np.arange(eligible.sum())
    columns = variable[indices].ravel()
    rows = np.repeat(np.arange(indices.size // 3), 3)
    mobile = columns >= 0
    return sparse.coo_matrix(
        (coefficients.ravel()[mobile], (rows[mobile], columns[mobile])),
        shape=(len(rows) // 3, eligible.sum()),
    ).tocsr()


def fit_cover_lift(
    matrix, rhs, bounds, vertices, faces, eligible, max_fill, cover_faces, weights
):
    if not eligible.any():
        return None
    columns = matrix.shape[1]
    cover = _lift_matrix(cover_faces, -weights, eligible)
    padded = sparse.vstack(
        [sparse.csr_matrix((matrix.shape[0] - cover.shape[0], cover.shape[1])), cover]
    )
    active_faces = faces[eligible[faces].any(axis=1)]
    corners = np.broadcast_to(active_faces[:, None, :], (len(active_faces), 2, 3))
    coefficients = _gradient_weights(vertices[:, :2], active_faces)
    slope = _lift_matrix(corners, coefficients, eligible)
    slope = sparse.vstack((slope, -slope))
    joint = sparse.bmat(
        [[matrix, padded], [sparse.csr_matrix((slope.shape[0], columns)), slope]],
        format="csr",
    )
    margin = MAX_FILL_GRADE / np.sqrt(2)
    result = linprog(
        np.r_[np.zeros(columns), np.ones(cover.shape[1])],
        A_ub=joint,
        b_ub=np.r_[rhs, np.full(slope.shape[0], margin)],
        bounds=[*bounds, *[(0, max_fill)] * cover.shape[1]],
        method="highs",
        options={"time_limit": 2.0},
    )
    if not result.success:
        return None
    displacement = result.x[columns:]
    lifted = displacement > 0
    if not lifted.any():
        return None
    gradients = (slope[: slope.shape[0] // 2] @ displacement).reshape(-1, 2)
    if (
        np.max(np.linalg.norm(gradients, axis=1)) > MAX_FILL_GRADE + 1e-7
        or displacement.min() < 0
        or displacement.max() > max_fill + 1e-7
    ):
        return None
    count = (columns - 1) // 2
    floor = linprog(
        np.r_[np.zeros(count), np.ones(count), 0],
        A_ub=matrix,
        b_ub=rhs - padded @ displacement,
        bounds=bounds,
        method="highs",
        options={"time_limit": 2.0},
    )
    return (floor.x if floor.success else result.x[:columns]), CoverLift(
        np.flatnonzero(eligible)[lifted], displacement[lifted]
    )
