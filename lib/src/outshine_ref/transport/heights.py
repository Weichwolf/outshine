"""Bounded anisotropic height fitting on a camera-independent shared surface."""

from dataclasses import dataclass
from math import isfinite
import time
from types import MappingProxyType
from typing import Mapping

import numpy as np

from ..contracts import Mesh


@dataclass(frozen=True, slots=True)
class HeightBudget:
    """Offline solver limit and explicit input/iteration caps; no frame-time claim."""

    time_limit_s: float = 8.
    max_iterations: int = 100
    max_vertices: int = 65536
    max_faces: int = 65536

    def __post_init__(self):
        if not isfinite(self.time_limit_s) or self.time_limit_s < 0:
            raise ValueError("finite nonnegative solver time required")
        if any(type(value) is not int or value <= 0 for value in
               (self.max_iterations, self.max_vertices, self.max_faces)):
            raise ValueError("positive integer height-work caps required")


@dataclass(frozen=True, slots=True)
class HeightFit:
    mesh: Mesh | None
    report: Mapping[str, object]

    def __post_init__(self):
        object.__setattr__(self, "report", MappingProxyType(dict(self.report)))


def _gradient_weights(xy, faces):
    p = xy[faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    det = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(det == 0):
        raise ValueError("height fit needs nondegenerate projected faces")
    return np.stack((np.c_[u[:, 1] - v[:, 1], v[:, 1], -u[:, 1]],
                     np.c_[v[:, 0] - u[:, 0], -v[:, 0], u[:, 0]]), axis=1) / det[:, None, None]


def _face_gradients(xy, faces, heights):
    z = heights[faces]
    z = z - z[:, :1]
    return np.einsum("fij,fj->fi", _gradient_weights(xy, faces), z)

def _error_rows(target):
    from scipy import sparse

    count = len(target)
    matrix = sparse.eye(count, format="csc")
    bound = sparse.csc_matrix(-np.ones((count, 1)))
    return sparse.vstack((sparse.hstack((matrix, bound)), sparse.hstack((-matrix, bound)))).tocsc(), np.r_[target, -target]


def _gradient_matrix(faces, coefficients, count):
    from scipy import sparse

    rows = np.repeat(np.arange(coefficients.shape[0] * coefficients.shape[1]), 3)
    columns = np.broadcast_to(faces[:, None, :], coefficients.shape).ravel()
    return sparse.csc_matrix((coefficients.ravel(), (rows, columns)),
                             shape=(coefficients.shape[0] * coefficients.shape[1], count + 1))


def _conic_fit(xy, faces, target, frames, seconds, iterations, pins):
    import clarabel
    from scipy import sparse
    began = time.perf_counter()
    datum = float(np.median(target))
    shifted = target - datum
    error, bounds = _error_rows(shifted)
    coefficients = np.einsum("fki,fij->fkj", frames, _gradient_weights(xy, faces))
    cone_coefficients = np.concatenate((np.zeros((len(faces), 1, 3)), -coefficients), axis=1)
    matrix = _gradient_matrix(faces, cone_coefficients, len(target))
    right = np.c_[np.full(len(faces), 1.), np.zeros((len(faces), 2))].ravel()
    cones = [clarabel.NonnegativeConeT(len(bounds))] + [clarabel.SecondOrderConeT(3) for _ in faces]
    blocks, rhs = [error, matrix], [bounds, right]
    if pins:
        nodes, heights = zip(*pins.items())
        fixed = sparse.csc_matrix((np.ones(len(nodes)), (np.arange(len(nodes)), nodes)), shape=(len(nodes), len(target) + 1))
        blocks.append(fixed)
        rhs.append(np.asarray(heights) - datum)
        cones.append(clarabel.ZeroConeT(len(nodes)))
    objective = np.r_[np.zeros(len(target)), 1.]
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit = seconds
    settings.max_iter = iterations
    settings.max_threads = 1
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    setup_ms = (time.perf_counter() - began) * 1000
    solver = clarabel.DefaultSolver(sparse.csc_matrix((len(target) + 1, len(target) + 1)), objective,
                                   sparse.vstack(blocks).tocsc(), np.concatenate(rhs), cones, settings)
    solution = solver.solve()
    heights = np.asarray(solution.x[:-1]) + datum if str(solution.status) in ("Solved", "AlmostSolved", "MaxTime", "MaxIterations") else None
    return heights, dict(method="cone", solver_status=str(solution.status), setup_ms=setup_ms,
                          solver_ms=solution.solve_time * 1000, total_ms=(time.perf_counter() - began) * 1000,
                          iterations=solution.iterations, reported_primal_objective=solution.obj_val,
                          reported_dual_objective=solution.obj_val_dual)

def fit_surface_heights(surface: Mesh, grade_frames: np.ndarray, *,
                        budget: HeightBudget = HeightBudget(), fixed: Mapping[int, float] | None = None) -> HeightFit:
    """Minimize height-prior deviation under norm(frame @ triangle_gradient) <= 1.

    Frames map east/north gradients to normalized longitudinal/crossfall limits.
    One shared vertex owns each height. Fixed contacts use the same local Z datum.
    The caller supplies a conforming nonoverlapping XY surface. Failed or expired
    solves return no mesh unless an independent whole-face/contact check passes.
    Clarabel/SciPy are optional transport dependencies, loaded only for solving.
    Solver limits exclude measured preparation and may overrun inside one step.
    """
    began = time.perf_counter()
    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise ValueError("owned Mesh and explicit HeightBudget required")
    n, f = len(surface.vertices), len(surface.triangles)
    if n > budget.max_vertices or f > budget.max_faces:
        return HeightFit(None, dict(valid=False, reason="input_budget_exhausted", total_ms=(time.perf_counter() - began) * 1000))
    if budget.time_limit_s == 0:
        return HeightFit(None, dict(valid=False, reason="time_budget_exhausted", total_ms=(time.perf_counter() - began) * 1000))
    frames = np.array(grade_frames, dtype=float, copy=True)
    if not f or frames.shape != (f, 2, 2) or not np.isfinite(frames).all() or np.any(np.linalg.det(frames) == 0):
        raise ValueError("finite nonsingular 2x2 grade frame per nonempty face required")
    pins = dict(fixed or {})
    if any(not isinstance(i, (int, np.integer)) or i < 0 or i >= n or not isfinite(h) for i, h in pins.items()):
        raise ValueError("fixed contacts must use existing vertex IDs and finite heights")
    xy, faces, target = surface.vertices[:, :2], surface.triangles, surface.vertices[:, 2]
    gradients = _face_gradients(xy, faces, target)
    normalized = np.einsum("fki,fi->fk", frames, gradients)
    if np.linalg.norm(normalized, axis=1).max() <= 1. + 1e-7 and all(abs(target[i] - h) <= 1e-7 for i, h in pins.items()):
        return HeightFit(surface, dict(valid=True, reason="", solver_status="prior_feasible", maximum_height_change_m=0.,
                                       maximum_face_grade=float(np.linalg.norm(gradients, axis=1).max()),
                                       maximum_normalized_grade=float(np.linalg.norm(normalized, axis=1).max()),
                                       maximum_pin_error_m=max((abs(target[i] - h) for i, h in pins.items()), default=0.), iterations=0,
                                       total_ms=(time.perf_counter() - began) * 1000))
    heights, report = _conic_fit(xy, faces, target, frames, budget.time_limit_s, budget.max_iterations, pins)
    reasons = {"MaxTime": "time_budget_without_valid_surface", "MaxIterations": "iteration_budget_without_valid_surface",
               "PrimalInfeasible": "infeasible_contacts_or_grade_constraints", "NumericalError": "solver_numerical_failure",
               "InsufficientProgress": "solver_insufficient_progress"}
    report.update(valid=False, reason=reasons.get(report["solver_status"], "no_valid_solver_result"), solver_time_limit_s=budget.time_limit_s,
                  max_iterations=budget.max_iterations)
    report = {key: value for key, value in report.items() if not isinstance(value, float) or isfinite(value)}
    if heights is not None and np.isfinite(heights).all():
        gradients = _face_gradients(xy, faces, heights)
        maximum = float(np.linalg.norm(np.einsum("fki,fi->fk", frames, gradients), axis=1).max())
        pin_error = max((abs(heights[i] - h) for i, h in pins.items()), default=0.)
        report.update(maximum_face_grade=float(np.linalg.norm(gradients, axis=1).max()),
                      maximum_normalized_grade=maximum, maximum_pin_error_m=pin_error,
                      maximum_height_change_m=float(np.max(abs(heights - target))))
        report["reason"] = "whole_face_or_contact_check_failed"
        if maximum <= 1. + 1e-7 and pin_error <= 1e-7:
            vertices = surface.vertices.copy()
            vertices[:, 2] = heights
            report.update(valid=True, reason="", total_ms=(time.perf_counter() - began) * 1000)
            return HeightFit(Mesh(vertices, surface.triangles), report)
    report["total_ms"] = (time.perf_counter() - began) * 1000
    return HeightFit(None, report)
