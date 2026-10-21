"""Independent SVD and displaced-point checks for the projective error bound."""

import unittest

import numpy as np
from terrain_projection_bound import displacement_bound, row_sum_bound, spectral_bound


class TerrainProjectionBound(unittest.TestCase):
    def test_bound_contains_displaced_points_for_general_projective_rows(self):
        rng = np.random.default_rng(515)
        viewport = np.array([1280, 720])
        for _ in range(100):
            rows = rng.normal(size=(3, 3))
            low = rng.uniform(-3, 0, (1, 2))
            high = rng.uniform(0, 3, (1, 2))
            minimum_w = rng.uniform(0.01, 200)
            error = minimum_w / (10 * np.linalg.norm(rows[2]))
            scale = spectral_bound(rows, low, high, viewport)[0]
            bound = error * scale / (minimum_w - error * np.linalg.norm(rows[2]))
            ndc = rng.uniform(low[0], high[0], (1000, 2))
            w = rng.uniform(minimum_w, 3 * minimum_w, 1000)
            delta = rng.normal(size=(1000, 3))
            delta *= (error * rng.uniform(0, 1, 1000) / np.linalg.norm(delta, axis=1))[:, None]
            clip_delta = np.einsum("ni,ji->nj", delta, rows)
            after = (ndc * w[:, None] + clip_delta[:, :2]) / (w + clip_delta[:, 2])[:, None]
            actual = np.linalg.norm((after - ndc) * viewport / 2, axis=1)
            self.assertTrue(np.isfinite(actual).all())
            self.assertLessEqual(actual.max(), bound)

    def test_float32_evaluation_encloses_independent_double_svd_at_all_corners(self):
        rng = np.random.default_rng(328)
        viewport = np.array([1280, 720], dtype=np.float32)
        for _ in range(1000):
            rows = (rng.normal(size=(3, 3)) * 10 ** rng.uniform(-8, 8)).astype(np.float32)
            low = rng.uniform(-3, 0, (1, 2)).astype(np.float32)
            high = rng.uniform(0, 3, (1, 2)).astype(np.float32)
            actual = spectral_bound(rows, low, high, viewport)[0]
            expected = 0
            for x in (low[0, 0], high[0, 0]):
                for y in (low[0, 1], high[0, 1]):
                    matrix = (rows[:2].astype(float) - np.array([[x], [y]]) * rows[2].astype(float)) * viewport[:, None] / 2
                    expected = max(expected, np.linalg.svd(matrix, compute_uv=False)[0])
            self.assertGreaterEqual(actual, expected)
            old = row_sum_bound(rows.astype(float), low.astype(float), high.astype(float), viewport.astype(float))[0]
            self.assertLessEqual(actual, old * (1 + 66 * np.finfo(np.float32).eps))

    def test_orthographic_diagonal_uses_largest_scale_and_unsafe_w_keeps_fine(self):
        rows = np.diag([2, 3, 0])
        low, high = np.array([[-1, -1]]), np.array([[1, 1]])
        viewport = np.array([1280, 720])
        actual = spectral_bound(rows, low, high, viewport)[0]
        self.assertGreaterEqual(actual, 1280)
        self.assertLess(actual, 1280.02)
        result = displacement_bound(np.array([[0, 2.0], [0, 2.0]]), np.array([1, 4]), 1, np.array([1280, 1280]))
        self.assertTrue(np.isinf(result[0, 1]))
        self.assertEqual(result[1, 1], 1280)
        np.testing.assert_array_equal(result[:, 0], 0)

    def test_underflow_does_not_turn_nonzero_projection_into_zero_error(self):
        rows = np.diag([1e-30, 1e-30, 0]).astype(np.float32)
        low, high = np.full((1, 2), -1, dtype=np.float32), np.ones((1, 2), dtype=np.float32)
        result = spectral_bound(rows, low, high, np.array([1280, 720], dtype=np.float32))
        self.assertTrue(np.isinf(result[0]))


if __name__ == "__main__":
    unittest.main()
