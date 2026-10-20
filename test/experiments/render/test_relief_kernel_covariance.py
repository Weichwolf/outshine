"""Independent finite-moment and unchanged guarded-response controls (#938)."""

import unittest

import numpy as np
from relief_covariance_reference import converged_moments
from relief_finite_kernel import point_response, transform
from relief_kernel_covariance import kernel_covariance, residual, response
from relief_shading_reference import direction


class KernelCovariance(unittest.TestCase):
    def test_independent_positive_quadrature_square_shear_thin_translation(self):
        polygons = (
            np.array([[-1.0, -1], [1, -1], [1, 1], [-1, 1]]),
            np.array([[-1.0, -0.3], [0.7, -0.4], [1, 0.6], [-0.6, 0.9]]),
            np.array([[0.6, 0.3], [0.63, 0.31], [0.61, 0.36]]),
        )
        for vertices in polygons:
            for roughness in (0.2, 0.9):
                for azimuth in (0, 90):
                    transformed = transform(
                        roughness, np.array([direction(-35, azimuth)]), direction(50)
                    )
                    mass, mean, covariance, _, _ = kernel_covariance(
                        vertices, transformed
                    )
                    expected, _ = converged_moments(
                        vertices, roughness, transformed[-1][0]
                    )
                    np.testing.assert_allclose(
                        mass[0], expected[0], atol=2e-11, rtol=2e-9
                    )
                    np.testing.assert_allclose(mean[0], expected[1], atol=2e-9, rtol=0)
                    np.testing.assert_allclose(
                        covariance[0], expected[2], atol=2e-9, rtol=2e-7
                    )
                    self.assertGreaterEqual(
                        np.linalg.eigvalsh(covariance[0]).min(), -2e-9
                    )

    def test_tangent_axis_permutation_preserves_tensor(self):
        vertices = np.array([[-1.0, -0.3], [0.7, -0.4], [1, 0.6], [-0.6, 0.9]])
        lights = np.array([direction(-35, 90), direction(65)])
        permutation = [1, 0, 2]
        original = kernel_covariance(vertices, transform(0.5, lights, direction(50)))
        swapped = kernel_covariance(
            vertices[::-1, ::-1],
            transform(0.5, lights[:, permutation], direction(50)[permutation]),
        )
        np.testing.assert_allclose(original[0], swapped[0], atol=2e-11, rtol=2e-9)
        np.testing.assert_allclose(original[1], swapped[1][:, ::-1], atol=2e-9, rtol=0)
        np.testing.assert_allclose(
            original[2], swapped[2][:, ::-1, ::-1], atol=2e-9, rtol=2e-7
        )

    def test_quadratic_expectation_matches_independent_area_integral(self):
        vertices = np.array([[-1.0, -0.3], [0.7, -0.4], [1, 0.6], [-0.6, 0.9]])
        tr = transform(0.5, np.array([direction(65)]), direction(50))
        mass, mean, covariance, _, _ = kernel_covariance(vertices, tr)
        (m, centre, cov), _ = converged_moments(vertices, 0.5, tr[-1][0])
        # Independent area moments supply the integral of (1 + 2*x - y)^2.
        vector = np.array([2.0, -1.0])
        expected = m * ((1 + centre @ vector) ** 2 + vector @ cov @ vector)
        actual = mass[0] * (
            (1 + mean[0] @ vector) ** 2 + vector @ covariance[0] @ vector
        )
        self.assertAlmostEqual(actual, expected, delta=2e-9)

    def test_constant_distributions_remain_exact_with_no_covariance_work(self):
        gradient = np.array([[2.0, 0], [0, 0]])
        coverage = np.array([0.4, 0.6])
        lights = np.array([direction(a, 90) for a in (-80, -35, 0, 65, 80)])
        actual, work, error = response(
            (gradient, coverage, [], {}), 0.2, lights, direction(50)
        )
        np.testing.assert_array_equal(
            actual, coverage @ point_response(gradient, 0.2, lights, direction(50))
        )
        self.assertEqual(work, {"constant_brdf_evaluations": 10})
        self.assertEqual(error, 0)

    def test_negative_view_and_half_guards_match_original_native_response(self):
        gradient = np.array([[2.0, 0], [0, 0], [-3.0, 1]])
        lights = np.array([direction(-35, 90), direction(65), direction(80, 90)])
        view = direction(50)
        half = transform(0.2, lights, view)[-1]
        value = residual(gradient, 0.2, lights, view, half)
        quadratic = (
            1
            + np.sum(gradient * gradient, axis=1)
            + (0.2**4 - 1) * (half[:, 2] - np.sum(half[:, :2] * gradient, axis=1)) ** 2
        )
        expected = np.diag(point_response(gradient, 0.2, lights, view))
        np.testing.assert_allclose(
            value * 0.2**4 / (np.pi * quadratic**2), expected, rtol=2e-15, atol=0
        )
        self.assertGreater(expected[0], 0)

    def test_clockwise_covariance_is_rejected(self):
        vertices = np.array([[-1.0, -1], [1, -1], [1, 1], [-1, 1]])
        with self.assertRaises(ValueError):
            kernel_covariance(
                vertices[::-1], transform(0.5, np.array([direction(0)]), direction(50))
            )


if __name__ == "__main__":
    unittest.main()
