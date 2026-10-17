"""Independent angular and ownership-free diffuse distribution counterexamples."""

import unittest

import numpy as np
from needle_response import (
    exact_response,
    mean_response,
    quadratic_moments,
    quadratic_response,
)


class NeedleResponseChecks(unittest.TestCase):
    def test_same_mean_different_response(self):
        a = np.array([[np.sqrt(3) / 2, 0, 0.5], [-np.sqrt(3) / 2, 0, 0.5]])
        b = a[:, [1, 0, 2]]
        w = np.array([0.5, 0.5])
        np.testing.assert_array_equal(w @ a, w @ b)
        self.assertAlmostEqual(exact_response(a, w, [1, 0, 0]), np.sqrt(3) / 4)
        self.assertEqual(exact_response(b, w, [1, 0, 0]), 0)
        self.assertEqual(mean_response(a, w, [1, 0, 0]), mean_response(b, w, [1, 0, 0]))
        self.assertGreater(
            quadratic_response(quadratic_moments(a, w), [1, 0, 0]),
            quadratic_response(quadratic_moments(b, w), [1, 0, 0]),
        )

    def test_quadratic_is_polynomial_not_exact_cosine(self):
        n = np.array([[0.0, 0.0, 1.0]])
        m = quadratic_moments(n, [1])
        self.assertAlmostEqual(quadratic_response(m, [1, 0, 0]), 3 / 32)
        self.assertEqual(exact_response(n, [1], [1, 0, 0]), 0)
        self.assertAlmostEqual(quadratic_response(m, [0, 0, 1]), 17 / 16)
        self.assertAlmostEqual(quadratic_response(m, [0, 0, -1]), 1 / 16)

    def test_scalar_polynomial_and_rotation(self):
        rng = np.random.default_rng(643)
        normals = rng.normal(size=(37, 3))
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        weights = rng.random(37) / 37
        rotation, _ = np.linalg.qr(rng.normal(size=(3, 3)))
        moments = quadratic_moments(normals, weights)
        for direction in np.eye(3).tolist() + [[-0.6, 0, 0.8]]:
            dots = normals @ direction
            oracle = sum(
                w * (3 / 32 + 0.5 * d + 15 / 32 * d * d)
                for w, d in zip(weights, dots, strict=True)
            )
            self.assertAlmostEqual(
                quadratic_response(moments, direction), oracle, places=14
            )
            self.assertAlmostEqual(
                exact_response(normals, weights, direction),
                exact_response(
                    normals @ rotation, weights, np.array(direction) @ rotation
                ),
                places=14,
            )

    def test_zero_gaps_and_linear_merge(self):
        normals = np.eye(3)
        self.assertEqual(exact_response(normals, [0, 0, 0], [1, 0, 0]), 0)
        np.testing.assert_array_equal(
            quadratic_moments(normals, [0, 0, 0]), np.zeros(9)
        )
        a = quadratic_moments(normals[:1], [0.25])
        b = quadratic_moments(normals[1:], [0, 0.5])
        np.testing.assert_allclose(
            a + b, quadratic_moments(normals, [0.25, 0, 0.5]), atol=1e-16
        )
        self.assertEqual(exact_response(normals, [0.25, 0, 0.5], [1, 0, 0]), 0.25)

    def test_invalid_inputs(self):
        for normals, weights in [
            ([[0, 0, 0]], [1]),
            ([[0, 0, 1]], [-1]),
            ([[0, 0, 1]], [float("nan")]),
        ]:
            with self.assertRaises(ValueError):
                quadratic_moments(normals, weights)


if __name__ == "__main__":
    unittest.main()
