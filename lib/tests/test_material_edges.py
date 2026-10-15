"""Independent piecewise Gaussian integration of metric material transitions."""

import math
import unittest
from itertools import pairwise

import numpy as np
from outshine_ref.materials import filtered_edge_weight


def mask(distance, reach):
    if reach == 0:
        return 1.0 if distance > 0 else 0.0 if distance < 0 else 0.5
    u = min(max(0.5 + 0.5 * distance / reach, 0.0), 1.0)
    return u * u * (3.0 - 2.0 * u)


def quadrature(distance, reach, half):
    if half == 0:
        return mask(distance, reach)
    low, high = distance - half, distance + half
    knots = sorted({low, high, min(max(-reach, low), high), min(max(reach, low), high)})
    integral = 0.0
    for left, right in pairwise(knots):
        centre, radius = 0.5 * (left + right), 0.5 * (right - left)
        integral += radius * sum(
            mask(centre + sign * radius / math.sqrt(3), reach) for sign in (-1, 1)
        )
    return integral / (2 * half)


class MaterialEdgeTests(unittest.TestCase):
    def test_filter_matches_independent_cubic_quadrature(self):
        rng = np.random.default_rng(173)
        reach = np.tile(np.array([0.0, 0.1, 0.3, 0.6, 0.9, 1.5], dtype=np.float32), 300)
        half = np.exp(rng.uniform(math.log(1e-6), math.log(1e6), len(reach))).astype(
            np.float32
        )
        distance = (rng.uniform(-1.2, 1.2, len(reach)) * (reach + half)).astype(
            np.float32
        )
        expected = np.array(
            [
                quadrature(float(d), float(b), float(h))
                for d, b, h in zip(distance, reach, half, strict=True)
            ]
        )
        actual = filtered_edge_weight(distance, reach, half)
        np.testing.assert_allclose(actual, expected, atol=8e-7, rtol=0.0)
        np.testing.assert_allclose(
            actual + filtered_edge_weight(-distance, reach, half),
            1.0,
            atol=1e-7,
            rtol=0.0,
        )
        assert np.all((actual >= 0) & (actual <= 1))

    def test_point_mask_hard_edges_broadcast_and_caller_ownership(self):
        distance = np.linspace(-3.0, 3.0, 601, dtype=np.float32)
        saved = distance.copy()
        for reach in (0.0, 0.3, 1.5):
            actual = filtered_edge_weight(distance, reach, 0.0)
            np.testing.assert_allclose(
                actual, [mask(float(d), reach) for d in distance], atol=3e-7, rtol=0.0
            )
            assert np.all(np.diff(actual) >= -1e-7)
        values = filtered_edge_weight(
            distance[:, None], np.array([0.0, 0.3, 1.5]), 0.01
        )
        assert values.shape == (601, 3) and values.dtype == np.float32
        np.testing.assert_array_equal(distance, saved)
        assert filtered_edge_weight(0.0, 0.3, 1000.0) == 0.5
        assert filtered_edge_weight(0.0, 0.0, 0.0) == 0.5

    def test_invalid_material_boundaries_are_rejected(self):
        cases = [
            (-1.0, -0.1, 0.2),
            (1.0, 0.1, -0.2),
            (math.nan, 1.0, 1.0),
            (0.0, math.inf, 1.0),
            (0.0, 1.0, math.nan),
            (1e100, 1.0, 1.0),
        ]
        for values in cases:
            with self.subTest(values=values), self.assertRaises(ValueError):
                filtered_edge_weight(*values)
