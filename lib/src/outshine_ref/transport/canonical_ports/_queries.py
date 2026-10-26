"""Bounded original-domain field/ray queries and source-oriented port frames."""

import math

import numpy as np

from ._geodesy import frame, positions_in_frame


class CoarseDomain:
    def __init__(self, surface, face_parts):
        self.parts = face_parts
        self.triangles = surface.vertices[surface.triangles, :2]
        self.lo = self.triangles.min(axis=1)
        self.hi = self.triangles.max(axis=1)
        self.edge1 = self.triangles[:, 1] - self.triangles[:, 0]
        self.edge2 = self.triangles[:, 2] - self.triangles[:, 0]
        self.determinant = (
            self.edge1[:, 0] * self.edge2[:, 1] - self.edge1[:, 1] * self.edge2[:, 0]
        )
        self.queries = 0
        self.face_bounds_tests = 0

    def part_at(self, xy):
        self.queries += 1
        if self.queries > 4096:
            raise ValueError("coarse field query budget")
        self.face_bounds_tests += len(self.triangles)
        candidates = np.flatnonzero(((xy >= self.lo) & (xy <= self.hi)).all(axis=1))
        delta = xy - self.triangles[candidates, 0]
        e1, e2 = self.edge1[candidates], self.edge2[candidates]
        det = self.determinant[candidates]
        u = (delta[:, 0] * e2[:, 1] - delta[:, 1] * e2[:, 0]) / det
        v = (e1[:, 0] * delta[:, 1] - e1[:, 1] * delta[:, 0]) / det
        inside = (u >= -1e-12) & (v >= -1e-12) & (u + v <= 1 + 1e-12)
        found = candidates[inside]
        if not len(found):
            raise ValueError("port ray leaves bounded original coarse surface")
        return int(self.parts[found].min())


class CoarseFieldQuery:
    def __init__(self, source, plan):
        self.plan = plan
        self.domain = CoarseDomain(source.surface, source.face_parts)

    @property
    def queries(self):
        return self.domain.queries

    @property
    def face_bounds_tests(self):
        return self.domain.face_bounds_tests

    def evaluate(self, xy):
        part = self.domain.part_at(xy)
        heights, gradients = self.plan.evaluate(
            np.asarray(xy)[None, :], np.array([part])
        )
        return float(heights[0]), gradients[0]


def canonical_port(
    query,
    source_anchor,
    port_anchor,
    original_segment_geo,
    half_width_m,
    initial_model_height,
):
    origin, source_axes = frame(source_anchor)
    target, target_axes = frame(port_anchor)
    at = source_axes.T @ (target - origin)
    up = source_axes.T @ target_axes[:, 2]
    height = float(initial_model_height)
    if not math.isfinite(height):
        raise ValueError("finite bound initial model height required")
    history = []
    for iteration in range(8):
        point = at + height * up
        value, gradient = query.evaluate(point[:2])
        residual = float(point[2] - value)
        history.append(residual)
        derivative = float(up[2] - gradient @ up[:2])
        if not math.isfinite(derivative) or derivative <= 0.25:
            raise ValueError("coarse surface is not upward in requested port frame")
        if abs(residual) <= 1e-8:
            break
        height -= residual / derivative
        if not math.isfinite(height) or abs(height) > 1e6:
            raise ValueError("bounded model port-height domain exceeded")
    else:
        raise ValueError("bounded port-ray iterations exhausted")
    normal = target_axes.T @ source_axes @ np.r_[-gradient, 1.0]
    plane = -normal[:2] / normal[2]
    endpoints = positions_in_frame(
        port_anchor, np.asarray(original_segment_geo), np.zeros(2)
    )
    direction = endpoints[1, :2] - endpoints[0, :2]
    length = math.hypot(*direction)
    if not math.isfinite(length) or length <= 1e-9:
        raise ValueError("original source direction is unqualified")
    direction /= length
    left = np.array([-direction[1], direction[0]])
    return {
        "anchor_lon_lat": list(map(float, port_anchor)),
        "east_m": 0.0,
        "north_m": 0.0,
        "height_m": height,
        "direction_EN": direction.tolist(),
        "plane_EN": plane.tolist(),
        "half_width_m": float(half_width_m),
        "longitudinal_grade": float(plane @ direction),
        "crossfall_rad": -math.atan(float(plane @ left)),
        "ray_iterations": iteration + 1,
        "ray_residuals_m": history,
    }
