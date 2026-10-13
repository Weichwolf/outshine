"""Independent polynomial identities, ownership and inverse counterexamples."""

import unittest

import numpy as np
from outshine_ref.vegetation import Profile
from outshine_ref.vegetation.bezier_curve import BezierCurve


class ProfileTests(unittest.TestCase):
    def test_prepared_table_owns_immutable_values(self):
        source = np.linspace(0.2, 0.8, 17)
        profile = Profile(source)
        source[:] = 99
        np.testing.assert_array_equal(profile.table, np.linspace(0.2, 0.8, 17))
        with self.assertRaises(ValueError):
            profile.table.setflags(write=True)
        with self.assertRaises(ValueError):
            profile.table[0] = 1
        np.testing.assert_array_equal(profile.evaluate([0, 0.5, 1]), [0.2, 0.5, 0.8])

    def test_linear_table_tiling_addition_clamp_and_derivative(self):
        primary = Profile(np.linspace(0.2, 0.8, 17))
        extra = Profile(np.linspace(-0.1, 0.3, 9))
        x = np.array([-2.75, -1, 0, 0.17, 0.9, 1, 2.35])
        np.testing.assert_allclose(
            primary.evaluate(x, tiled=True, extra=extra), 0.1 + x, atol=1e-15
        )
        np.testing.assert_allclose(
            primary.evaluate(x, extra=extra, clamp=True),
            np.clip(0.1 + np.clip(x, 0, 1), 0, 1),
            atol=1e-15,
        )
        np.testing.assert_allclose(
            primary.derivative(x), np.where((x < 0) | (x > 1), 0, 0.6), atol=1e-15
        )
        np.testing.assert_array_equal(primary.evaluate(np.empty((0, 2))).shape, (0, 2))

    def test_invalid_prepared_values_are_rejected(self):
        for values in (
            ["1"] * 4,
            [True] * 4,
            [1j] * 4,
            [1, 2, np.inf, 3],
            [1, 2, np.nan, 3],
            [1, 2, 3],
            np.ones((2, 2)),
        ):
            with self.subTest(values=values), self.assertRaises(ValueError):
                Profile(values)
        with self.assertRaises(ValueError):
            Profile(np.ones(4), kind="linear")
        for query in (True, "0.5", 1j, np.nan):
            with self.subTest(query=query), self.assertRaises(ValueError):
                Profile(np.ones(4)).evaluate(query)


class BezierTests(unittest.TestCase):
    def test_raw_matches_independent_de_casteljau_in_two_segments(self):
        points = np.array(
            [
                [[0, 1], [0.1, 0.2], [0.3, 0.4], [0.5, 0.5]],
                [[0.5, 0.5], [0.6, 0.7], [0.9, 0.1], [1, 0]],
            ]
        )
        curve = BezierCurve(points)
        points[:] = 0
        queries = np.r_[-0.1, np.linspace(0, 1, 127), 1.1]
        expected = []
        for t in np.clip(queries, 0, 1):
            index = min(int(t * 2), 1)
            local = t * 2 - index
            values = np.asarray(curve.controls[index]).copy()
            for _ in range(3):
                values = (1 - local) * values[:-1] + local * values[1:]
            expected.append(values[0])
        np.testing.assert_allclose(curve.raw(queries), expected, atol=5e-16)

    def test_public_polynomial_mutation_cannot_change_curve(self):
        curve = BezierCurve((((0, 0), (0.3, 0.2), (0.7, 0.8), (1, 1)),))
        queries = np.linspace(0, 1, 53)
        before = curve.value(queries)
        exposed = curve.polynomial
        exposed.c[..., 1] = 0
        exposed.x[0] = -1
        np.testing.assert_array_equal(curve.value(queries), before)
        np.testing.assert_array_equal(curve.polynomial.x, [0, 1])

    def test_flat_endpoint_inverse_retains_small_positive_inputs(self):
        curve = BezierCurve((((0, 0), (0, 1 / 3), (0, 2 / 3), (1, 1)),))
        x = np.r_[0, np.logspace(-18, -1, 71), 1]
        np.testing.assert_allclose(curve.value(x), np.cbrt(x), atol=1e-13, rtol=0)
        np.testing.assert_allclose(
            curve.reference_value(x), np.cbrt(x), atol=1e-13, rtol=0
        )
        self.assertGreater(float(curve.value(1e-15)), 0)

    def test_crossed_handles_are_valid_but_negative_derivatives_are_not(self):
        crossed = BezierCurve((((0, 0), (0.8, 0.2), (0.2, 0.8), (1, 1)),))
        np.testing.assert_allclose(crossed.value([0, 0.5, 1]), [0, 0.5, 1], atol=1e-13)
        reversing = BezierCurve((((0, 0), (1.5, 0.2), (-0.5, 0.8), (1, 1)),))
        with self.assertRaisesRegex(ValueError, "monotone"):
            reversing.value(0.5)
        with self.assertRaises(ValueError):
            BezierCurve((((0, 0), (0.3, 0.2), (0.7, 0.8), (0.9, 1)),)).value(0.5)


if __name__ == "__main__":
    unittest.main()
