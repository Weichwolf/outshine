import unittest

import numpy as np
from ground_litter_polynomial import cell_weights


class PolynomialFilter(unittest.TestCase):
    def test_matches_all_nine_gauss_samples_in_one_cell(self):
        rng = np.random.default_rng(1934)
        at = rng.uniform(-8, 8, (16384, 2))
        dx, dy = rng.uniform(-0.5, 0.5, (2, len(at), 2))
        inside, actual = cell_weights(at, dx, dy)
        self.assertGreater(np.count_nonzero(inside), 2048)
        f = at[inside] - np.floor(at[inside])
        expected = np.zeros((len(f), 4))
        for x, wx in zip((-np.sqrt(0.15), 0, np.sqrt(0.15)), (5 / 18, 8 / 18, 5 / 18)):
            for y, wy in zip(
                (-np.sqrt(0.15), 0, np.sqrt(0.15)), (5 / 18, 8 / 18, 5 / 18)
            ):
                q = f + x * dx[inside] + y * dy[inside]
                q = q * q * (3 - 2 * q)
                sx, sy = q.T
                expected += (
                    wx
                    * wy
                    * np.column_stack(
                        ((1 - sx) * (1 - sy), sx * (1 - sy), (1 - sx) * sy, sx * sy)
                    )
                )
        np.testing.assert_allclose(actual[inside], expected, rtol=0, atol=2e-15)
        self.assertGreaterEqual(actual[inside].min(), -2e-15)
        np.testing.assert_allclose(actual.sum(axis=1), 1, rtol=0, atol=2e-15)

    def test_point_limit_and_negative_coordinates(self):
        at = np.array([[-3.25, -4.75], [0, 0], [-1, -1]], dtype=float)
        inside, weights = cell_weights(at, np.zeros_like(at), np.zeros_like(at))
        np.testing.assert_array_equal(inside, True)
        np.testing.assert_array_equal(weights[1:], [[1, 0, 0, 0]] * 2)
        np.testing.assert_allclose(
            weights[0], [0.1318359375, 0.7119140625, 0.0244140625, 0.1318359375]
        )

    def test_crossed_cell_is_not_admitted(self):
        at = np.array([[0, 0], [0.99, 0.5], [-0.99, -0.5]])
        inside, _ = cell_weights(at, np.tile([0.1, 0], (3, 1)), np.zeros_like(at))
        np.testing.assert_array_equal(inside, False)

    def test_rejects_nonfinite_and_mismatched_queries(self):
        for at in (np.zeros((2, 3)), np.array([[np.nan, 0]])):
            with self.assertRaises(ValueError):
                cell_weights(at, np.zeros((1, 2)), np.zeros((1, 2)))


if __name__ == "__main__":
    unittest.main()
