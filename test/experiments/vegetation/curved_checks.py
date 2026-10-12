"""Polynomial stationary-point references for cubic branch envelopes."""

import numpy as np
from fork_field import smooth_maximum
from numpy.polynomial import polynomial as poly


def continuous_reference(spines, profiles, blend, points):
    scores = np.empty((len(points), len(spines)))
    gradients = np.empty((len(points), len(spines), 3))
    for branch, (spine, profile) in enumerate(zip(spines, profiles, strict=True)):
        controls = np.asarray(spine.controls)
        if len(controls) != 1:
            raise ValueError("reference requires a single cubic segment per branch")
        p = controls[0]
        coefficients = np.array(
            (
                p[0],
                3 * (p[1] - p[0]),
                3 * (p[2] - 2 * p[1] + p[0]),
                p[3] - 3 * p[2] + 3 * p[1] - p[0],
            )
        )
        radii = profile.evaluate(np.linspace(0, 1, 17))
        np.testing.assert_allclose(
            radii, np.linspace(radii[0], radii[-1], len(radii)), atol=1e-14
        )
        radius = np.array((radii[0], radii[-1] - radii[0]))
        for index, point in enumerate(points):
            difference = coefficients.copy()
            difference[0] -= point
            polynomial = np.zeros(7)
            radius_squared = poly.polymul(radius, radius)
            polynomial[: len(radius_squared)] = radius_squared
            for axis in range(3):
                squared = poly.polymul(difference[:, axis], difference[:, axis])
                polynomial[: len(squared)] -= squared
            roots = poly.polyroots(poly.polyder(polynomial))
            real = roots.real[
                (abs(roots.imag) < 1e-9) & (roots.real >= 0) & (roots.real <= 1)
            ]
            parameter = np.concatenate(([0.0, 1.0], real))
            values = poly.polyval(parameter, polynomial)
            chosen = int(np.argmax(values))
            scores[index, branch] = values[chosen]
            centre = poly.polyval(parameter[chosen], coefficients)
            gradients[index, branch] = -2 * (point - centre)
    values, weights = smooth_maximum(scores, blend)
    return values, np.sum(weights[:, :, None] * gradients, axis=1)


def curve_checks(field, spines, profiles):
    rng = np.random.default_rng(23523)
    points = rng.uniform(-1, 1, (41, 3))
    expected, _gradient = continuous_reference(spines, profiles, field.blend, points)
    actual, gradient = field.evaluate(points, True)
    bounds = []
    for plan, spine in zip(field.branches, spines, strict=True):
        distance = np.max(
            np.linalg.norm(points[:, None] - np.asarray(spine.controls)[0], axis=2),
            axis=1,
        )
        e = plan.source_error_bound_m
        bounds.append(2 * (distance + max(plan.radii)) * e + 2 * e**2)
    assert np.all(abs(expected - actual) <= np.max(bounds, axis=0) + 1e-13)
    numerical = np.column_stack(
        [
            (
                field.evaluate(points + axis * 1e-6)
                - field.evaluate(points - axis * 1e-6)
            )
            / 2e-6
            for axis in np.eye(3)
        ]
    )
    np.testing.assert_allclose(gradient, numerical, rtol=1e-7, atol=1e-8)
    minima, maxima = points - 0.05, points + 0.05
    low, high = field.block_intervals(minima, maxima)
    for _ in range(8):
        values = field.evaluate(rng.uniform(minima, maxima))
        assert np.all(values >= low) and np.all(values <= high)
    return {
        "polynomial_reference_points": len(points),
        "maximum_observed_field_error_m2": float(np.max(abs(expected - actual))),
        "maximum_gradient_error_m": float(np.max(abs(gradient - numerical))),
        "source_envelope_bound_m": max(p.source_error_bound_m for p in field.branches),
        "interval_probes": len(points) * 8,
    }


def surface_checks(mesh, spines, profiles, blend):
    vertices, faces = mesh["vertices"], mesh["faces"]
    vertex_indices = np.linspace(0, len(vertices) - 1, 64, dtype=int)
    face_indices = np.linspace(0, len(faces) - 1, 64, dtype=int)
    samples = np.vstack(
        (vertices[vertex_indices], np.mean(vertices[faces[face_indices]], axis=1))
    )
    projected = samples.copy()
    for _ in range(6):
        values, gradient = continuous_reference(spines, profiles, blend, projected)
        squared = np.sum(gradient**2, axis=1)
        assert np.min(squared) > 0
        projected -= (values / squared)[:, None] * gradient
    residual, _gradient = continuous_reference(spines, profiles, blend, projected)
    assert np.max(abs(residual)) < 1e-10
    return {
        "continuous_surface_probes": len(samples),
        "maximum_observed_projection_m": float(
            np.max(np.linalg.norm(projected - samples, axis=1))
        ),
        "scope": "sampled displacement to polynomial-reference surface; not a global Hausdorff distance or a guaranteed closest-point solve",
    }


def plan_regressions():
    from curved_fork import CurveBranch, CurvedFork
    from profile_curve import Profile
    from spine_curve import CubicSpine
    from tapered_experiment import fixtures

    points = np.random.default_rng(23524).uniform(-1, 1, (97, 3))
    cases = 0
    for original in fixtures().values():
        expected, expected_gradient = original.evaluate(points, True)
        for count in (1, 4, 16):
            parameter = np.linspace(0, 1, count + 1)
            plans = tuple(
                CurveBranch(parameter[:, None] * v, (1 - parameter) * a + parameter * b)
                for v, a, b in zip(
                    np.asarray(original.endpoints),
                    original.radii,
                    original.tip_radii,
                    strict=True,
                )
            )
            field = CurvedFork(plans, original.blend)
            values, gradient = field.evaluate(points, True)
            np.testing.assert_allclose(values, expected, atol=2e-14, rtol=0)
            np.testing.assert_allclose(gradient, expected_gradient, atol=2e-13, rtol=0)
            cases += 1
    bowed = CubicSpine((((0, 0, 0), (0.5, 0.3, 0), (0.5, 0.6, 0), (0, 0.9, 0)),))
    branch = CurveBranch.from_spine(bowed, Profile(np.full(4, 0.04)), tolerance=0.001)
    field = CurvedFork((branch,), 0)
    values = field.evaluate(
        np.column_stack((np.zeros(1001), np.linspace(0, 1, 1001), np.zeros(1001)))
    )
    crossings = int(np.count_nonzero(np.signbit(values[:-1]) != np.signbit(values[1:])))
    assert crossings == 3
    try:
        CurveBranch.from_spine(
            bowed, Profile(np.full(4, 0.04)), tolerance=0.001, max_segments=1
        )
    except ValueError:
        pass
    else:
        raise AssertionError("insufficient source approximation budget accepted")
    return {
        "straight_subdivision_invariance_cases": cases,
        "nonstar_ray_crossings": crossings,
        "insufficient_source_budget_rejected": True,
    }


def reference_surface_points(spines, profiles, blend):
    from spine_frames import unit

    samples = []
    for spine, profile in zip(spines, profiles, strict=True):
        t = np.linspace(0, 1, 5)
        centres, tangent, speed, _curvature = spine.sample(t)
        radii = profile.evaluate(t)
        slope = (profile.table[-1] - profile.table[0]) / speed
        assert np.max(abs(slope)) < 1
        for c, axis, radius, rate in zip(centres, tangent, radii, slope, strict=True):
            basis = unit(np.cross(axis, np.eye(3)[np.argmin(abs(axis))]))
            second = np.cross(axis, basis)
            angle = np.arange(8) * np.pi / 4
            lateral = np.cos(angle)[:, None] * basis + np.sin(angle)[:, None] * second
            samples.extend(c + radius * (np.sqrt(1 - rate**2) * lateral - rate * axis))
        samples.extend(
            (centres[0] - radii[0] * tangent[0], centres[-1] + radii[-1] * tangent[-1])
        )
    samples = np.asarray(samples)
    for _ in range(12):
        value, gradient = continuous_reference(spines, profiles, blend, samples)
        denominator = np.sum(gradient**2, axis=1)
        assert np.min(denominator) > 0
        samples -= (value / denominator)[:, None] * gradient
    residual, _gradient = continuous_reference(spines, profiles, blend, samples)
    assert np.max(abs(residual)) < 1e-10
    return samples


def distance_checks():
    from mesh_distance import point_mesh_distances

    mesh = {
        "vertices": np.array(((0, 0, 0), (1, 0, 0), (0, 1, 0)), dtype=float),
        "faces": np.array(((0, 1, 2),)),
    }
    points = np.array(((0.25, 0.25, 1), (2, 0, 0), (-1, -1, 0), (1, 1, 0), (0, 0, 0)))
    expected = np.array((1, 1, np.sqrt(2), np.sqrt(0.5), 0))
    np.testing.assert_allclose(point_mesh_distances(mesh, points), expected, atol=1e-14)
    return {"analytic_point_triangle_cases": len(points)}
