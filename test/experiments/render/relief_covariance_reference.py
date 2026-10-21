"""Independent positive area quadrature of the original finite slope kernel."""

import numpy as np
from relief_shading_reference import triangle_rule
from shapely.geometry import Polygon


def positive_moments(vertices, roughness, half, order):
    raw = np.zeros(6)
    for index in range(1, len(vertices) - 1):
        a, b, c = vertices[[0, index, index + 1]]
        ab, bc, ca = (a + b) / 2, (b + c) / 2, (c + a) / 2
        for triangle in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)):
            points, weights = triangle_rule(Polygon(triangle), order)
            quadratic = (
                1
                + np.sum(points * points, axis=1)
                + (roughness**4 - 1) * (half[2] - points @ half[:2]) ** 2
            )
            weighted = weights * roughness**4 / (np.pi * quadratic**2)
            x, y = points.T
            raw += (
                np.column_stack([np.ones(len(x)), x, y, x * x, x * y, y * y]).T
                @ weighted
            )
    return raw


def central_moments(raw):
    mean = raw[1:3] / raw[0]
    second = np.array([[raw[3], raw[4]], [raw[4], raw[5]]]) / raw[0]
    return raw[0], mean, second - np.outer(mean, mean)


def converged_moments(vertices, roughness, half):
    previous = None
    for order in (64, 128, 256, 512):
        current = positive_moments(vertices, roughness, half, order)
        if previous is not None and np.max(abs(current - previous)) <= 1e-10:
            return central_moments(current), order
        previous = current
    raise ValueError("independent positive moment integration did not converge")
