"""Cheap constant-plane control with the same full-width samples/grade ellipses."""

import numpy as np

from ._queries import CoarseDomain


class AffineGround:
    def __init__(self, source):
        xyz = source.surface.vertices
        self.origin = xyz[:, :2].mean(axis=0)
        xy = xyz[:, :2] - self.origin
        datum = float(np.median(xyz[:, 2]))
        matrix = np.c_[np.ones(len(xy)), xy]
        coefficients = np.linalg.lstsq(matrix, xyz[:, 2] - datum, rcond=None)[0]
        gradient = coefficients[1:]
        norm = float(np.linalg.norm(source.grade_frames @ gradient, axis=1).max())
        if norm > 1:
            gradient = gradient / norm
            for _ in range(8):
                if np.linalg.norm(source.grade_frames @ gradient, axis=1).max() <= 1:
                    break
                gradient = np.nextafter(gradient, np.zeros(2))
            else:
                raise ValueError("affine grade rounding guard unresolved")
        residual = xyz[:, 2] - xy @ gradient
        self.intercept = float((residual.min() + residual.max()) / 2)
        self.gradient = gradient
        self.domain = CoarseDomain(source)
        error = float(np.max(abs(self.intercept + xy @ gradient - xyz[:, 2])))
        self.report = {
            "method": "least-squares gradient with common grade scaling and exact fixed-gradient minimax intercept",
            "maximum_original_prior_error_m": error,
            "maximum_normalized_continuous_grade": float(
                np.linalg.norm(source.grade_frames @ gradient, axis=1).max()
            ),
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
