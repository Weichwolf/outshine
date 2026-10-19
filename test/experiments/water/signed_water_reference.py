"""Independent nearest-ellipsoid normals for near-surface signed-water diagnostics."""

import numpy as np
from outshine_ref.places.camera import axes, ecef
from outshine_ref.water.geodetic import WGS84_A, WGS84_E2
from scipy.optimize import brentq


def nearest_normals(anchor, points, *, counters=None):
    """Solve the ellipsoid's scalar Lagrange multiplier; never call the PROJ inverse."""
    frame = axes(anchor[1], anchor[0])
    world = np.asarray(points) @ frame + ecef(anchor[1], anchor[0], 0)
    square_axes = np.array([1, 1, 1 - WGS84_E2])
    scaled = world / WGS84_A
    multipliers = []
    for point in scaled:

        def residual(value, point=point):
            return float(
                np.sum(square_axes * point**2 / (square_axes + value) ** 2) - 1
            )

        if residual(-0.02) <= 0 or residual(0.02) >= 0:
            raise ValueError(
                "diagnostic nearest-point bracket only covers near-surface points"
            )
        root, result = brentq(
            residual,
            -0.02,
            0.02,
            xtol=np.nextafter(0.0, 1.0),
            rtol=4 * np.finfo(float).eps,
            full_output=True,
        )
        multipliers.append(root)
        if counters is not None:
            counters["independent_nearest_points"] = (
                counters.get("independent_nearest_points", 0) + 1
            )
            counters["root_residual_calls"] = (
                counters.get("root_residual_calls", 0) + result.function_calls + 2
            )
    multiplier = np.array(multipliers)
    closest = world * square_axes / (square_axes + multiplier[:, None])
    normal = closest / square_axes
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    height = np.sign(multiplier) * np.linalg.norm(world - closest, axis=1)
    return normal @ frame.T, height


def prism_points(xy, bounds, subdivisions=8):
    bary = (
        np.array(
            [
                [i, j, subdivisions - i - j]
                for i in range(subdivisions + 1)
                for j in range(subdivisions + 1 - i)
            ]
        )
        / subdivisions
    )
    return np.array(
        [[x, y, z] for x, y in bary @ xy for z in (*bounds, np.mean(bounds))]
    )
