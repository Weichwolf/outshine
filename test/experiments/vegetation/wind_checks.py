"""Independent scalar displacement and finite-difference wind contracts."""

import math

import numpy as np
from wind_field import WindField


def check_rejections(field):
    invalid = (
        lambda: field.bind(np.broadcast_to(np.zeros(3), (1_000_001, 3))),
        lambda: field.bind(np.array([[np.nan, 0, 0]])),
        lambda: field.coefficients(np.nan),
        lambda: field.coefficients(0, 1.01),
        lambda: WindField(**(field.arrays() | {"amplitudes": field.amplitudes * 10})),
        lambda: WindField(**(field.arrays() | {"direction": np.array([0, 0, 1])})),
    )
    for call in invalid:
        try:
            call()
        except ValueError:
            continue
        raise AssertionError("invalid wind request accepted")
    assert all(
        not a.flags.writeable
        for a in field.arrays().values()
        if isinstance(a, np.ndarray)
    )
    return len(invalid)


def scalar_position(field, point, seconds, strength):
    def step(x):
        x = max(0, min(1, x))
        return x * x * (3 - 2 * x)

    h = max(0, point[2] / field.height)
    value = 0.0
    for i, amplitude in enumerate(field.amplitudes):
        if i == 0:
            weight = h * h if h <= 1 else 2 * h - 1
        else:
            r = [point[k] - field.origins[i, k] for k in range(3)]
            along = sum(r[k] * field.tangents[i, k] for k in range(3))
            radial2 = sum((r[k] - along * field.tangents[i, k]) ** 2 for k in range(3))
            weight = (
                step(h)
                * step(along / field.lengths[i])
                * max(0, 1 - radial2 / field.supports[i] ** 2) ** 3
            )
        value += (
            strength
            * amplitude
            * math.sin(2 * math.pi * field.frequencies[i] * seconds + field.phases[i])
            * weight
        )
    return np.array([point[k] + value * field.direction[k] for k in range(3)])


def check_wind(field, mesh, anchors):
    rng = np.random.default_rng(138)
    points = np.vstack(
        (
            mesh["vertices"][rng.choice(len(mesh["vertices"]), 128, replace=False)],
            rng.uniform((-0.6, -0.6, -0.2), (0.6, 0.6, 2), (128, 3)),
            anchors,
            np.zeros((1, 3)),
        )
    )
    binding = field.bind(points)
    error = normal_error = derivative_error = 0.0
    determinant_min = 1.0
    normals = rng.normal(size=points.shape)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    for seconds in (0.0, 0.5, 2.0, 10.0, -0.75):
        moved, gradient = field.move(points, binding, seconds)
        reference = np.array([scalar_position(field, p, seconds, 1) for p in points])
        error = max(error, float(np.max(np.abs(moved - reference))))
        numeric = np.empty((len(points), 3, 3))
        for axis in range(3):
            delta = np.eye(3)[axis] * 1e-6
            numeric[:, :, axis] = np.array(
                [
                    (
                        scalar_position(field, p + delta, seconds, 1)
                        - scalar_position(field, p - delta, seconds, 1)
                    )
                    / 2e-6
                    for p in points
                ]
            )
        jacobian = np.eye(3) + field.direction[None, :, None] * gradient[:, None, :]
        derivative_error = max(
            derivative_error, float(np.max(np.abs(numeric - jacobian)))
        )
        expected = np.linalg.solve(numeric.swapaxes(1, 2), normals[..., None])[..., 0]
        expected /= np.linalg.norm(expected, axis=1)[:, None]
        normal_error = max(
            normal_error,
            float(np.max(np.abs(field.normals(normals, gradient) - expected))),
        )
        determinant_min = min(determinant_min, float(np.linalg.det(jacobian).min()))
        assert np.array_equal(moved[-1], np.zeros(3))
        still, flat = field.move(points, binding, seconds, 0)
        assert np.array_equal(still, points) and not flat.any()
        reverse, _ = field.move(points[::-1], field.bind(points[::-1]), seconds)
        assert np.allclose(reverse[::-1], moved, rtol=0, atol=1e-15)
    assert error < 1e-14 and derivative_error < 2e-7 and normal_error < 2e-7
    assert determinant_min >= field.determinant_lower_bound - 1e-12
    return {
        "sample_points": len(points),
        "times": 5,
        "scalar_position_max_error_m": error,
        "finite_difference_jacobian_max_error": derivative_error,
        "finite_difference_normal_max_error": normal_error,
        "sampled_determinant_min": determinant_min,
        "global_determinant_lower_bound": field.determinant_lower_bound,
    }
