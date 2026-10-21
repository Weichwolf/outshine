"""Finite-cohort numerical bank distances for an explicit capped bed recipe."""

import math

import numpy as np
from scipy.spatial import cKDTree


class BankDistance:
    def __init__(self, segments, *, prepare_index=True):
        if (
            not isinstance(segments, np.ndarray)
            or segments.ndim != 3
            or segments.shape[1:] != (2, 3)
            or not 0 < len(segments) <= 8192
        ):
            raise ValueError("bounded finite bank segments required")
        self.segments = np.array(segments, dtype=float, copy=True)
        if not np.isfinite(self.segments).all() or np.any(abs(self.segments) > 1e8):
            raise ValueError("bounded finite bank segments required")
        self.delta = self.segments[:, 1] - self.segments[:, 0]
        self.length2 = np.einsum("ij,ij->i", self.delta, self.delta)
        if np.any(self.length2 <= 0):
            raise ValueError("positive bank segments required")
        self.centres = self.segments.mean(axis=1) if prepare_index else None
        self.half_length = math.sqrt(float(self.length2.max())) / 2
        self.margin = 64 * math.ulp(max(1.0, float(abs(self.segments).max())))
        self.tree = cKDTree(self.centres) if prepare_index else None
        for array in (self.segments, self.delta, self.length2):
            array.setflags(write=False)
        if self.centres is not None:
            self.centres.setflags(write=False)

    def query(self, points, *, cap_m, method="index", max_tests=2000000):
        """Midpoint ball + maximum half-length encloses every candidate segment.

        This is a numerical broadphase checked against scan, not a rigorous
        binary64 certificate. Cap is the explicit taper radius, not an IO bound.
        """
        if (
            not isinstance(points, np.ndarray)
            or points.ndim != 2
            or points.shape[1] != 3
            or not 0 < len(points) <= 16384
        ):
            raise ValueError("bounded points, cap and distance work required")
        points = np.asarray(points, float)
        if (
            not np.isfinite(points).all()
            or np.any(abs(points) > 1e8)
            or not math.isfinite(cap_m)
            or not 0 < cap_m <= 1e6
            or method not in ("scan", "index")
            or type(max_tests) is not int
            or not 0 <= max_tests <= 10000000
        ):
            raise ValueError("bounded points, cap and distance work required")
        if method == "scan" and len(points) * len(self.segments) > max_tests:
            raise ValueError("bank segment-test budget exhausted")
        if method == "index" and self.tree is None:
            raise ValueError("bank index was not prepared")
        result = np.full(len(points), float(cap_m))
        work = {"point_queries": len(points), "returned_indices": 0, "segment_tests": 0}
        for i, point in enumerate(points):
            ids = (
                list(range(len(self.segments)))
                if method == "scan"
                else sorted(
                    self.tree.query_ball_point(
                        point, self.half_length + cap_m + self.margin
                    )
                )
            )
            work["returned_indices"] += len(ids)
            if work["segment_tests"] + len(ids) > max_tests:
                raise ValueError("bank segment-test budget exhausted")
            work["segment_tests"] += len(ids)
            if not ids:
                continue
            start, delta = self.segments[ids, 0], self.delta[ids]
            station = np.clip(
                np.einsum("ij,ij->i", point - start, delta) / self.length2[ids], 0, 1
            )
            result[i] = min(
                cap_m,
                float(
                    np.linalg.norm(
                        point - (start + station[:, None] * delta), axis=1
                    ).min()
                ),
            )
        return result, work
