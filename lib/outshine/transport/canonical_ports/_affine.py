"""Cheap constant-plane control with the same full-width samples/grade ellipses."""

import numpy as np

from ._queries import CoarseDomain


def fit_samples(xyz, grade_frames):
    """Fit already validated full-domain samples; this grants no domain coverage."""
    origin = xyz[:, :2].mean(axis=0)
    xy = xyz[:, :2] - origin
    datum = float(np.median(xyz[:, 2]))
    matrix = np.c_[np.ones(len(xy)), xy]
    coefficients = np.linalg.lstsq(matrix, xyz[:, 2] - datum, rcond=None)[0]
    gradient = coefficients[1:]
    norm = float(np.linalg.norm(grade_frames @ gradient, axis=1).max())
    if norm > 1:
        gradient = gradient / norm
        for _ in range(8):
            if np.linalg.norm(grade_frames @ gradient, axis=1).max() <= 1:
                break
            gradient = np.nextafter(gradient, np.zeros(2))
        else:
            raise ValueError("affine grade rounding guard unresolved")
    residual = xyz[:, 2] - xy @ gradient
    intercept = float((residual.min() + residual.max()) / 2)
    error = float(np.max(abs(intercept + xy @ gradient - xyz[:, 2])))
    normalized_grade = float(np.linalg.norm(grade_frames @ gradient, axis=1).max())
    return origin, intercept, gradient, error, normalized_grade


class AffineGround:
    def __init__(self, source):
        self.origin, self.intercept, self.gradient, error, norm = fit_samples(
            source.surface.vertices, source.grade_frames
        )
        self.domain = CoarseDomain(source.surface, source.face_parts)
        self.report = {
            "method": "least-squares gradient with common grade scaling and exact fixed-gradient minimax intercept",
            "maximum_original_prior_error_m": error,
            "maximum_normalized_continuous_grade": norm,
            "unknowns": 3,
            "finite_mesh_constructed": False,
            "global_optimality_claimed": False,
            "scope": "Analytic affine control on the same source domain and original full-width DEM observations. No dense-DEM or frame/source-closure shortcut.",
        }

    def evaluate(self, xy):
        self.domain.part_at(xy)
        return float(self.intercept + (xy - self.origin) @ self.gradient), self.gradient

    @property
    def queries(self):
        return self.domain.queries

    @property
    def face_bounds_tests(self):
        return self.domain.face_bounds_tests
