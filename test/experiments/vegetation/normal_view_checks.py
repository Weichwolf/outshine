"""Independent quad/view UV mapping preserves local footprint orientation and inputs."""

import unittest

import numpy as np
from normal_view_uv import isolated_uv


class ViewUVTests(unittest.TestCase):
    def test_all_views_preserve_local_quad_and_positive_orientation(self):
        views = np.arange(64)
        local = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], float)
        origin = np.c_[views % 8 * 40 + 4, views // 8 * 40 + 4]
        original = ((local[None] * 32 + origin[:, None]) / 320).reshape(-1, 2)
        before = original.copy()
        mapped = isolated_uv(original, views).reshape(-1, 4, 2)
        expected = local[None] + np.c_[views % 10, views // 10][:, None]
        np.testing.assert_allclose(mapped, expected, atol=1e-14, rtol=0)
        np.testing.assert_array_equal(before, original)
        edge1 = mapped[:, 1] - mapped[:, 0]
        edge2 = mapped[:, 3] - mapped[:, 0]
        np.testing.assert_allclose(
            edge1[:, 0] * edge2[:, 1] - edge1[:, 1] * edge2[:, 0], 1
        )
        self.assertFalse(np.shares_memory(mapped, original))

    def test_noncorner_queries_and_declared_view_mismatch(self):
        local = np.array([[0.1, 0.2], [0.8, 0.2], [0.8, 0.7], [0.1, 0.7]])
        original = (local * 32 + [44, 44]) / 320
        mapped = isolated_uv(original, np.array([9]))
        np.testing.assert_allclose(mapped, local + [9, 0], atol=1e-14)
        with self.assertRaisesRegex(ValueError, "declared original"):
            isolated_uv(original, np.array([0]))

    def test_invalid_and_oversized_packets_refuse(self):
        for uv, views in [
            (np.zeros((4, 2)), np.array([64])),
            (np.zeros((4, 2)), np.array([0.5])),
            (np.zeros((3, 2)), np.array([0])),
            (np.zeros((0, 2)), np.array([], int)),
            (np.zeros((4, 2)) + np.nan, np.array([0])),
        ]:
            with self.assertRaises(ValueError):
                isolated_uv(uv, views)
        with self.assertRaises(ValueError):
            isolated_uv(np.empty((0, 2)), np.zeros(80001, int))


if __name__ == "__main__":
    unittest.main()
