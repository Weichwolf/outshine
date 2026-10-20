"""Independent guarded-response and finite positive-range controls (#948)."""

import unittest

import numpy as np
from relief_finite_kernel import kernel_moments, point_response, transform
from relief_residual_range import (
    residual_range,
    response,
    split,
    visibility_times_light,
)
from relief_shading_reference import direction, triangle_rule
from shapely.geometry import Polygon


class ResidualRangeTests(unittest.TestCase):
    def setUp(self):
        self.vertices = np.array([[-0.4, -0.2], [0.3, -0.25], [0.35, 0.4], [-0.3, 0.3]])
        self.light, self.view = direction(65, 90), direction(50)

    def original_residual(self, points, roughness, light, view):
        half = (light + view) / np.linalg.norm(light + view)
        quadratic = (
            1
            + np.sum(points * points, axis=1)
            + (roughness**4 - 1) * (half[2] - points @ half[:2]) ** 2
        )
        return point_response(points, roughness, light[None], view)[:, 0] / (
            roughness**4 / (np.pi * quadratic**2)
        )

    def test_collapsed_domain_matches_original_guarded_response(self):
        # Includes actual negative view/half-dot cases, not visibility removal.
        points = np.array([[2.0, 0], [-3, 1], [0, 0], [0.3, -0.4]])
        for roughness in (0.2, 0.9, 1.0):
            expected = self.original_residual(points, roughness, self.light, self.view)
            half = transform(roughness, self.light[None], self.view)[-1][0]
            for point, value in zip(points, expected, strict=True):
                bounds = residual_range(
                    point[None], roughness, self.light, self.view, half
                )
                np.testing.assert_allclose(bounds, value, rtol=3e-14, atol=1e-14)
        self.assertGreater(expected[0], 0)

    def test_range_contains_original_samples_with_guard_crossings(self):
        polygons = [self.vertices, np.array([[-3.0, -1], [3, -1], [3, 1], [-3, 1]])]
        random = np.random.default_rng(948)
        for vertices in polygons:
            bary = random.dirichlet(np.ones(4), 1000)
            points = np.r_[vertices, bary @ vertices]
            for roughness in (0.2, 0.9):
                half = transform(roughness, self.light[None], self.view)[-1][0]
                bounds = residual_range(
                    vertices, roughness, self.light, self.view, half
                )
                values = self.original_residual(
                    points, roughness, self.light, self.view
                )
                self.assertLessEqual(bounds[0], values.min() + 1e-12)
                self.assertGreaterEqual(bounds[1], values.max() - 1e-12)

    def test_visibility_times_light_has_required_monotonicity(self):
        for a2 in (0.2**4, 0.9**4, 1.0):
            light = np.linspace(0, 1, 51)[None]
            view = np.geomspace(1e-6, 1, 41)[:, None]
            values = visibility_times_light(light, view, a2)
            self.assertTrue((np.diff(values, axis=1) >= -1e-12).all())
            self.assertTrue((np.diff(values, axis=0) <= 1e-12).all())

    def test_tangent_axis_permutation_preserves_bounds(self):
        half = transform(0.2, self.light[None], self.view)[-1][0]
        original = residual_range(self.vertices, 0.2, self.light, self.view, half)
        swap = [1, 0, 2]
        changed = residual_range(
            self.vertices[::-1, ::-1],
            0.2,
            self.light[swap],
            self.view[swap],
            half[swap],
        )
        np.testing.assert_allclose(original, changed, atol=1e-12, rtol=1e-13)

    def test_split_preserves_area_and_kernel_mass(self):
        children = split(self.vertices)
        self.assertAlmostEqual(
            sum(Polygon(v).area for v in children),
            Polygon(self.vertices).area,
            delta=1e-15,
        )
        tr = transform(0.2, self.light[None], self.view)
        mass = kernel_moments(self.vertices, tr)[0][0]
        actual = sum(kernel_moments(v, tr)[0][0] for v in children)
        self.assertAlmostEqual(actual, mass, delta=2e-11 + 2e-9 * mass)

    def test_independent_positive_area_integral_lies_in_range(self):
        density = 1 / Polygon(self.vertices).area
        model = (np.zeros((1, 2)), np.zeros(1), [(self.vertices, density)], {})
        value, bounds, work = response(model, 0.9, self.light, self.view, leaf_limit=8)
        before = None
        for order in (32, 64):
            points, weights = triangle_rule(Polygon(self.vertices), order)
            expected = float(
                density
                * weights
                @ point_response(points, 0.9, self.light[None], self.view)[:, 0]
            )
            if before is not None:
                self.assertAlmostEqual(expected, before, delta=1e-11)
            before = expected
        self.assertLessEqual(bounds[0], expected + 1e-11)
        self.assertGreaterEqual(bounds[1], expected - 1e-11)
        self.assertGreaterEqual(value, 0)
        self.assertEqual(work["leaves"], 8)
        self.assertFalse(work["model_tolerance_met"])

    def test_constant_distribution_remains_exact(self):
        gradient = np.array([[2.0, 0], [0, 0]])
        coverage = np.array([0.4, 0.6])
        model = gradient, coverage, [], {}
        value, bounds, work = response(model, 0.2, self.light, self.view)
        expected = float(
            coverage @ point_response(gradient, 0.2, self.light[None], self.view)[:, 0]
        )
        self.assertEqual(value, expected)
        np.testing.assert_array_equal(bounds, [expected, expected])
        self.assertTrue(work["model_tolerance_met"])
        self.assertEqual(work.get("range_cells", 0), 0)

    def test_tiny_support_can_meet_criterion_without_reference_values(self):
        vertices = self.vertices * 1e-5
        model = (
            np.zeros((1, 2)),
            np.zeros(1),
            [(vertices, 1 / Polygon(vertices).area)],
            {},
        )
        value, bounds, work = response(model, 0.9, self.light, self.view)
        self.assertEqual(work["leaves"], 1)
        self.assertTrue(work["model_tolerance_met"])
        self.assertLessEqual((bounds[1] - bounds[0]) / 2, 0.004 * bounds[0])
        self.assertGreater(value, 0)

    def test_invalid_budgets_and_undefined_half_are_rejected(self):
        model = np.zeros((1, 2)), np.ones(1), [], {}
        for keyword in ({"leaf_limit": 0}, {"relative": 0}, {"leaf_limit": 1025}):
            with self.assertRaises(ValueError):
                response(model, 0.2, self.light, self.view, **keyword)
        with self.assertRaisesRegex(ValueError, "defined half"):
            response(model, 0.2, self.view, -self.view)


if __name__ == "__main__":
    unittest.main()
