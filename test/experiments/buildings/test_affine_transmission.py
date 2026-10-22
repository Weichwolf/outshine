"""Individually evaluated log factors, including long/flat and reversed runs."""

import math
import unittest

import numpy as np
from affine_transmission import sum_log1m_affine


class AffineTransmissionTest(unittest.TestCase):
    def test_products_match_individual_factors(self):
        rng = np.random.default_rng(1113)
        largest = 0.0
        for n in [1, 2, 16, 17, 18, 33, 100, 612, 9812, 100000]:
            for a, b in [
                (0.0, 0.85),
                (0.85, 0.0),
                (0.5, 0.5),
                (1e-12, 2e-12),
                (0.5, 0.5 + 1e-12),
                *rng.uniform(0, 0.85, (8, 2)),
            ]:
                for tail in [0.02, 0.38, 1.0]:
                    q = np.linspace(a, b, n) if n > 1 else [a]
                    expected = math.fsum(math.log1p(-tail * float(v)) for v in q)
                    actual = float(sum_log1m_affine(a, b, n, tail))
                    largest = max(largest, abs(actual - expected))
                    self.assertLess(
                        abs(actual - expected), 3e-12 * max(1, abs(expected))
                    )
        self.assertLess(largest, 2e-9)

    def test_packet_zero_and_invalid_inputs(self):
        np.testing.assert_array_equal(
            sum_log1m_affine([0.1, 0.2], [0.8, 0.7], [0, 0], 1), [0, 0]
        )
        np.testing.assert_array_equal(sum_log1m_affine(0.85, 0.85, 1000000, 0), 0)
        np.testing.assert_allclose(
            sum_log1m_affine(0.25, 0.25, 1000000, 1),
            1000000 * math.log(0.75),
            rtol=1e-15,
        )
        with self.assertRaises(ValueError):
            sum_log1m_affine(-0.1, 0.5, 20, 1)


if __name__ == "__main__":
    unittest.main()
