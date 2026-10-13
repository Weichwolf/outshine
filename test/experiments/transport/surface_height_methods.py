"""Fit shared road vertex heights with constraints on entire affine triangles."""

import time

import numpy as np
from scipy import sparse
from scipy.optimize import linprog

from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget, fit_surface_heights, _gradient_weights, _face_gradients, _error_rows, _gradient_matrix


gradient_weights = _gradient_weights
face_gradients = _face_gradients
error_rows = _error_rows
gradient_matrix = _gradient_matrix


def face_slopes(xy, faces, heights):
    return np.linalg.norm(face_gradients(xy, faces, heights), axis=1)


def constrained_coefficients(xy, faces, frames):
    coefficients = gradient_weights(xy, faces)
    return np.einsum("fki,fij->fkj", frames, coefficients) if frames is not None else coefficients


def conic_fit(xy, faces, target, maximum_grade=.08, seconds=2., pins=None, frames=None):
    if frames is None:
        frames = np.broadcast_to(np.eye(2) / maximum_grade, (len(faces), 2, 2))
    result = fit_surface_heights(Mesh(np.c_[xy, target], faces), frames,
                                 budget=HeightBudget(time_limit_s=seconds), fixed=pins)
    return (result.mesh.vertices[:, 2] if result.mesh is not None else None), dict(result.report, method="cone")


def polygon_fit(xy, faces, target, maximum_grade=.08, seconds=2., pins=None, frames=None):
    began = time.perf_counter()
    datum = float(np.median(target))
    error, bounds = error_rows(target - datum)
    angle = np.arange(8) * np.pi / 8
    normals = np.c_[np.cos(angle), np.sin(angle)]
    coefficients = np.einsum("dk,fkj->fdj", normals, constrained_coefficients(xy, faces, frames))
    gradient = gradient_matrix(faces, coefficients, len(target))
    grade = np.full(len(faces) * 8, (maximum_grade if frames is None else 1.) * np.cos(np.pi / 16))
    blocks, rhs = [error, gradient, -gradient], [bounds, grade, grade]
    equality, equality_rhs = None, None
    if pins:
        nodes, heights = zip(*pins.items())
        equality = sparse.csc_matrix((np.ones(len(nodes)), (np.arange(len(nodes)), nodes)), shape=(len(nodes), len(target) + 1))
        equality_rhs = np.asarray(heights) - datum
    setup_ms = (time.perf_counter() - began) * 1000
    solve_start = time.perf_counter()
    result = linprog(np.r_[np.zeros(len(target)), 1.], A_ub=sparse.vstack(blocks).tocsc(), b_ub=np.concatenate(rhs),
                     A_eq=equality, b_eq=equality_rhs, bounds=[(None, None)] * len(target) + [(0, None)],
                     method="highs", options={"time_limit": seconds, "primal_feasibility_tolerance": 1e-9})
    heights = result.x[:-1] + datum if result.x is not None else None
    return heights, dict(method="polygon", solver_status=result.message, setup_ms=setup_ms,
                          solver_ms=(time.perf_counter() - solve_start) * 1000,
                          total_ms=(time.perf_counter() - began) * 1000,
                          iterations=result.nit, conservative_grade_factor=float(np.cos(np.pi / 16)))


def checked_fit(method, xy, faces, target, maximum_grade=.08, seconds=2., pins=None, frames=None):
    if not np.isfinite(maximum_grade) or maximum_grade <= 0 or not np.isfinite(seconds) or seconds <= 0:
        raise ValueError("finite positive grade and solver time limit required")
    heights, report = (conic_fit if method == "cone" else polygon_fit)(xy, faces, target, maximum_grade, seconds, pins, frames)
    report.update(valid=False, reason="no_valid_solver_result")
    if heights is not None and np.isfinite(heights).all():
        gradients = face_gradients(xy, faces, heights)
        maximum = float(np.linalg.norm(gradients, axis=1).max())
        normalized = np.einsum("fki,fi->fk", frames, gradients) if frames is not None else gradients / maximum_grade
        normalized_maximum = float(np.linalg.norm(normalized, axis=1).max())
        pin_error = max((abs(heights[i] - height) for i, height in (pins or {}).items()), default=0.)
        report.update(maximum_face_grade=maximum, maximum_normalized_grade=normalized_maximum, maximum_pin_error_m=pin_error,
                      maximum_height_change_m=float(np.max(abs(heights - target))))
        if normalized_maximum <= 1. + 1e-7 and pin_error <= 1e-7:
            report.update(valid=True, reason="")
            return heights, report
    return None, report
