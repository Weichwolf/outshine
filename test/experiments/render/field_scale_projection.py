"""Perspective position bounds, without coverage, material or visibility claims."""

import numpy as np
from trilinear_support import CORNERS


def camera_points(points, view):
    eye, basis, focal, width, height = view
    points = np.asarray(points, float)
    if (
        points.ndim != 2
        or points.shape[1] != 3
        or not np.isfinite(points).all()
        or not np.isfinite(eye).all()
        or not np.isfinite(basis).all()
        or not np.allclose(basis @ basis.T, np.eye(3), atol=1e-10, rtol=0)
        or not np.isfinite([focal, width, height]).all()
        or min(focal, width, height) <= 0
    ):
        raise ValueError("finite points and orthonormal positive pinhole view required")
    camera = (points - eye) @ basis.T
    if not len(camera) or np.any(camera[:, 2] <= 0.1):
        raise ValueError("nonempty geometry strictly beyond the near plane required")
    return camera


def project(points, view):
    camera = camera_points(points, view)
    return [view[3] / 2, view[4] / 2] + view[2] * camera[:, :2] / camera[:, 2, None] * [
        1,
        -1,
    ]


def box_envelope(bounds, view):
    bounds = np.asarray(bounds, float)
    if bounds.shape != (2, 3) or np.any(bounds[1] < bounds[0]):
        raise ValueError("ordered three dimensional box required")
    corners = bounds[0] + CORNERS * (bounds[1] - bounds[0])
    camera = camera_points(corners, view)
    depth = float(camera[:, 2].min())
    slope = float(np.linalg.norm(camera[:, :2] / camera[:, 2, None], axis=1).max())
    return depth, np.sqrt(1 + slope**2)


def displacement_bound(bounds, view, radius):
    """Bound f*e*sqrt(1+qmax^2)/(zmin-e), including perspective denominator.

    For camera point (u,z), the exact change is
    f*(delta_u-(u/z)*delta_z)/(z+delta_z). The numerator's operator norm
    is sqrt(1+||u/z||^2). Positive-depth box projection is the convex hull
    of its projected corners; their largest radial slope bounds the box.
    This is a real-arithmetic geometric inequality, not directed rounding.
    """
    depth, angular_factor = box_envelope(bounds, view)
    if not np.isfinite(radius) or not 0 <= radius < depth - 0.1:
        raise ValueError("nonnegative displacement must remain beyond the near plane")
    return float(view[2] * radius * angular_factor / (depth - radius))


def position_budget(bounds, view, pixels=0.5):
    depth, angular_factor = box_envelope(bounds, view)
    if not np.isfinite(pixels) or pixels <= 0:
        raise ValueError("positive finite pixel position allowance required")
    radius = pixels * depth / (view[2] * angular_factor + pixels)
    if radius >= depth - 0.1:
        raise ValueError("position allowance would cross the near plane")
    return float(radius)


def quantiles(values):
    values = np.asarray(values, float)
    if not np.isfinite(values).all():
        raise ValueError("finite diagnostic measurements required")
    return (
        np.quantile(values, [0, 0.25, 0.5, 0.75, 0.9, 1]).tolist()
        if len(values)
        else None
    )
