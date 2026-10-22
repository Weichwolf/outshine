"""Exact zero-contribution support, followed by the existing finite GGX estimate."""

from collections import Counter

import numpy as np
from relief_finite_kernel import kernel_moments, point_response, transform
from shapely.geometry import Polygon, box
from shapely.geometry.polygon import orient


def lit_polygon(vertices, light):
    """Clip convex slope support to Lz - Lxy.q >= 0, without renormalizing density."""
    values = light[2] - vertices @ light[:2]
    if values.min() >= 0:
        return vertices, "full"
    if values.max() <= 0:
        return np.empty((0, 2)), "empty"
    length = np.linalg.norm(light[:2])
    a, b = -light[:2] / length
    rotation = np.array([[a, b], [-b, a]])
    local = vertices @ rotation.T
    polygon = Polygon(local)
    _, low, high, top = polygon.bounds
    clipped = polygon.intersection(box(-light[2] / length, low, high, top))
    if clipped.is_empty or clipped.area == 0:
        return np.empty((0, 2)), "empty"
    if clipped.geom_type != "Polygon" or not clipped.is_valid:
        raise ValueError("unsupported clipped slope support")
    result = np.asarray(orient(clipped, sign=1).exterior.coords[:-1]) @ rotation
    return result, "partial"


def response(model, roughness, lights, view):
    """Current guarded response: clip light support only, never the view hemisphere."""
    gradient, coverage, corners, _ = model
    result = coverage @ point_response(gradient, roughness, lights, view)
    work = Counter(constant_brdf_evaluations=len(coverage) * len(lights))
    if not corners:
        return result, dict(work)
    transformed = transform(roughness, lights, view)
    for vertices, density in corners:
        for index, light in enumerate(lights):
            clipped, state = lit_polygon(vertices, light)
            work["support_tests"] += 1
            work["support_vertex_tests"] += len(vertices)
            work[state] += 1
            if not len(clipped):
                continue
            work["kernel_edges"] += len(clipped)
            work["residual_brdf_evaluations"] += 1
            if state == "partial":
                work["clipped_array_bytes"] += clipped.nbytes
                work["clipped_arrays"] += 1
            one = tuple(value[index : index + 1] for value in transformed)
            mass, centroid = kernel_moments(clipped, one)
            point = point_response(
                centroid, roughness, lights[index : index + 1], view
            )[0, 0]
            q = centroid[0]
            half = one[-1][0]
            quadratic = 1 + q @ q + (roughness**4 - 1) * (half[2] - half[:2] @ q) ** 2
            kernel = roughness**4 / (np.pi * quadratic**2)
            result[index] += density * mass[0] * point / kernel
    return result, dict(work)
