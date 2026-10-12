"""Independent scalar optimization and differential checks for tapered sphere envelopes."""

import numpy as np
from fork_field import ForkField
from scipy.optimize import minimize_scalar
from scipy.spatial.transform import Rotation
from tapered_fork import TaperedFork


def scalar_envelope(point, endpoint, base, tip):
    def objective(t):
        radius = (1 - t) * base + t * tip
        delta = point - t * endpoint
        return np.dot(delta, delta) - radius**2

    result = minimize_scalar(
        objective, bounds=(0, 1), method="bounded", options={"xatol": 1e-13}
    )
    assert result.success
    return -min(result.fun, objective(0), objective(1))


def envelope_checks():
    rng = np.random.default_rng(23520)
    points = rng.uniform(-2, 2, (83, 3))
    maximum_error, cases = 0.0, 0
    parameters = (
        ((0, 1, 0), 0.25, 0.04),
        ((0.3, 0.8, -0.4), 0.04, 0.3),
        ((0, 0.1, 0), 0.1, 0.5),
        ((0, 0.1, 0), 0.5, 0.1),
        ((0, 0, 0), 0.2, 0.3),
        ((0, 0, 0), 0.3, 0.2),
        ((0, 0.125, 0), 0.25, 0.125),
    )
    for endpoint, base, tip in parameters:
        field = TaperedFork((endpoint,), (base,), 0, tip_radii=(tip,))
        values = field.evaluate(points)
        expected = np.array(
            [scalar_envelope(p, np.array(endpoint), base, tip) for p in points]
        )
        error = float(np.max(abs(values - expected)))
        np.testing.assert_allclose(values, expected, rtol=0, atol=2e-13)
        maximum_error = max(maximum_error, error)
        cases += len(points)
    return {
        "scalar_optimization_points": cases,
        "maximum_scalar_error_m2": maximum_error,
    }


def field_checks(field):
    rng = np.random.default_rng(23521)
    points = field.origin + rng.uniform(-1, 1, (113, 3))
    values, gradient = field.evaluate(points, True)
    step = 1e-6
    numerical = np.column_stack(
        [
            (
                field.evaluate(points + axis * step)
                - field.evaluate(points - axis * step)
            )
            / (2 * step)
            for axis in np.eye(3)
        ]
    )
    np.testing.assert_allclose(gradient, numerical, atol=1e-8, rtol=1e-7)
    transform = Rotation.from_rotvec((0.5, -0.7, 0.3))
    moved = TaperedFork(
        transform.apply(field.endpoints) * 3,
        np.asarray(field.radii) * 3,
        field.blend * 9,
        transform.apply(field.origin) * 3 + (2, -4, 7),
        np.asarray(field.tip_radii) * 3,
    )
    changed, changed_gradient = moved.evaluate(
        transform.apply(points) * 3 + (2, -4, 7), True
    )
    np.testing.assert_allclose(changed, values * 9, atol=2e-13)
    np.testing.assert_allclose(
        changed_gradient, transform.apply(gradient) * 3, atol=2e-13
    )
    constant = ForkField(field.endpoints, field.radii, field.blend, field.origin)
    same = TaperedFork(
        field.endpoints, field.radii, field.blend, field.origin, field.radii
    )
    np.testing.assert_allclose(
        same.evaluate(points), constant.evaluate(points), atol=1e-15
    )
    minima = points - rng.uniform(0, 0.2, points.shape)
    maxima = points + rng.uniform(0, 0.2, points.shape)
    low, high = field.block_intervals(minima, maxima)
    for _ in range(12):
        samples = minima + rng.random(points.shape) * (maxima - minima)
        value = field.evaluate(samples)
        assert np.all(value >= low) and np.all(value <= high)
    return {
        "gradient_points": len(points),
        "maximum_gradient_error_m": float(np.max(abs(gradient - numerical))),
        "enclosed_random_points": len(points) * 12,
    }


def analytic_taper_checks():
    probes = 0
    for length, base, tip in ((2.0, 0.5, 0.1), (2.0, 0.1, 0.6), (0.5, 0.05, 0.5)):
        slope = (tip - base) / length
        parameter = np.linspace(0.1, 0.9, 37)
        radius = (1 - parameter) * base + parameter * tip
        normal = np.array([np.sqrt(1 - slope**2), -slope, 0])
        points = radius[:, None] * normal + np.column_stack(
            (np.zeros_like(parameter), parameter * length, np.zeros_like(parameter))
        )
        field = TaperedFork(((0, length, 0),), (base,), 0, tip_radii=(tip,))
        np.testing.assert_allclose(field.evaluate(points), 0, atol=2e-15)
        np.testing.assert_allclose(
            field.normals(points), np.broadcast_to(normal, points.shape), atol=2e-14
        )
        probes += len(points)
    return {"analytic_cone_tangent_points": probes}


def rejection_checks():
    from fork_refinement import project_vertices, refined_voxel_mesh, subdivide

    rejected = 0
    for tips in ((), (0,), (-0.1,), (float("nan"),), (float("inf"),), (0.1, 0.2)):
        try:
            TaperedFork(((0, 1, 0),), (0.2,), 0, tip_radii=tips)
        except ValueError:
            rejected += 1
        else:
            raise AssertionError("invalid tapered radius accepted")
    sphere = TaperedFork(((0, 0, 0),), (0.2,), 0, tip_radii=(0.2,))
    actions = (
        lambda: refined_voxel_mesh(sphere, rounds=3),
        lambda: project_vertices(sphere, np.zeros((1, 3)), 0.1),
        lambda: project_vertices(sphere, np.array([[0.3, 0, 0]]), 0.001),
        lambda: subdivide(
            np.zeros((1, 3)), np.broadcast_to(np.zeros((1, 3), dtype=int), (250001, 3))
        ),
    )
    for action in actions:
        try:
            action()
        except ValueError:
            rejected += 1
        else:
            raise AssertionError("invalid refinement request accepted")
    return {"rejected_invalid_inputs_and_budgets": rejected}
