"""Independent decimal complex-exponential and whole-cell interpolation controls."""

import itertools
import math
import unittest
from decimal import Decimal as D
from decimal import localcontext
from fractions import Fraction as F

import numpy as np
from road_normal_cone import NormalModel
from road_normal_math import polynomial_range, sqrt_interval, trig_point
from road_surface_model import Curve
from test_road_surface_lod import recipe


def decimal(value):
    value = value if isinstance(value, F) else F(float(value))
    return D(value.numerator) / D(value.denominator)


def oracle_trig(value):
    # (1+i*x/2^180)^(2^180), independent of the producer's Taylor sums.
    a, b = D(1), decimal(value) / D(2) ** 180
    for _ in range(180):
        a, b = a * a - b * b, 2 * a * b
    return b, a


def oracle_normal(curve, index, s, u):
    with localcontext() as context:
        context.prec = 140
        distance = decimal(F(s) - F(float(curve.stations[index])))
        length = decimal(curve.lengths[index])
        t = distance / length

        def bezier(control):
            n = len(control) - 1
            return sum(
                D(math.comb(n, j))
                * (t**j if j else D(1))
                * ((1 - t) ** (n - j) if n != j else D(1))
                * decimal(x)
                for j, x in enumerate(control)
            )

        def derivative(control):
            return [
                3 * (F(float(b)) - F(float(a))) / F(float(curve.lengths[index]))
                for a, b in itertools.pairwise(control)
            ]

        beta = bezier(curve.bank[index])
        sb, cb = oracle_trig(F(beta))
        bank = sb / cb
        rate = bezier(derivative(curve.bank[index])) * (1 + bank * bank)
        k = (
            decimal(curve.curvature_start[index])
            + decimal(curve.curvature_rate[index]) * distance
        )
        q = (bezier(derivative(curve.height[index])) - decimal(u) * rate) / (
            1 - decimal(u) * k
        )
        theta = (
            decimal(curve.poses[index, 4])
            + decimal(curve.curvature_start[index]) * distance
            + decimal(curve.curvature_rate[index]) * distance**2 / 2
        )
        sn, cs = oracle_trig(F(theta))
        v = (-q * cs - bank * sn, -q * sn + bank * cs, D(1))
        norm = sum(x * x for x in v).sqrt()
        return np.array([float(x / norm) for x in v])


class ExactRanges(unittest.TestCase):
    def test_reparameterized_bernstein_against_exact_power_formula(self):
        control = (F(1, 3), F(-4, 7), F(9, 2), F(1, 5))
        for a, b in ((F(0), F(1)), (F(1, 7), F(2, 7)), (F(999, 1000), F(1001, 1000))):
            low, high = polynomial_range(control, a, b)
            for j in range(65):
                t = a + (b - a) * F(j, 64)
                value = sum(
                    math.comb(3, k) * t**k * (1 - t) ** (3 - k) * v
                    for k, v in enumerate(control)
                )
                self.assertLessEqual(low, value)
                self.assertLessEqual(value, high)

    def test_rational_sqrt_bound_with_no_float_oracle(self):
        for value in (F(0), F(1, 7), F(10**100), F(1, 10**100)):
            low, high = sqrt_interval((value, value))
            self.assertLessEqual(low * low, value)
            self.assertGreaterEqual(high * high, value)

    def test_taylor_encloses_independent_decimal_complex_limit(self):
        with localcontext() as context:
            context.prec = 140
            for x in (-3.0, -1.57, -0.2, 0.0, 0.7, 1.0, 3.0):
                got = trig_point(x)
                reference = oracle_trig(F(x))
                for interval, value in zip(got, reference, strict=True):
                    self.assertLessEqual(interval[0], F(value))
                    self.assertLessEqual(F(value), interval[1])
        with self.assertRaisesRegex(ValueError, "work budget"):
            trig_point(1000)


class NormalCones(unittest.TestCase):
    def test_heading_reuse_keeps_cell_and_point_phases_distinct(self):
        curve = Curve(recipe(curvature=0.02, height=0.01, bank_rate=0.01))
        model = NormalModel(curve)
        first = model.cone(0, 0, 1, -2, -1, 0.1)
        model.cone(0, 0, 1, 1, 2, 0.1)
        self.assertEqual(model.work["heading_requests"], 2)
        self.assertEqual(model.work["heading_evaluations"], 1)
        model.cone(0, 0, 2, -2, -1, 0.1)
        self.assertEqual(model.work["heading_evaluations"], 2)
        self.assertEqual(model.cone(0, 0, 1, -2, -1, 0.1), first)
        self.assertEqual(model.work["heading_evaluations"], 3)
        a = model.stored_normal(0, 1, -1, 0.1)
        model.stored_normal(0, 1, 1, 0.1)
        self.assertEqual(model.work["point_phase_requests"], 2)
        self.assertEqual(model.work["point_phase_evaluations"], 1)
        model.stored_normal(0, 2, -1, 0.1)
        self.assertEqual(model.work["point_phase_evaluations"], 2)
        np.testing.assert_array_equal(model.stored_normal(0, 1, -1, 0.1), a)
        self.assertEqual(model.work["point_phase_evaluations"], 3)

    def test_wider_cone_preserves_positive_interpolation_budget(self):
        # A unit normal within r of the axis and stored normals within r+d
        # have mutual angular separation at most 2r+d, including after any
        # positive interpolation inside the convex cone (r+d < pi/2).
        for limit in (0.01, 0.2, 1.0):
            r, d = limit / 3, limit / 4 * 0.999
            source_vertex = np.array([math.sin(r), 0, math.cos(r)])
            direct = np.array([-math.sin(r), 0, math.cos(r)])
            stored = (
                math.cos(d) * np.array([math.sin(r + d), 0, math.cos(r + d)])
            ).astype(np.float32)
            self.assertLess(np.linalg.norm(stored - source_vertex), math.sin(limit / 4))
            interpolated = stored.astype(float) / np.linalg.norm(stored.astype(float))
            angle = math.atan2(
                np.linalg.norm(np.cross(interpolated, direct)), interpolated @ direct
            )
            self.assertLess(angle, limit)
            self.assertLessEqual(angle, 11 * limit / 12 + 1e-6)

    def test_wider_cone_accepts_previously_rejected_safe_cell(self):
        curve = Curve(recipe(height=0.01))
        model = NormalModel(curve)
        result = model.cone(0, 0, 3, -2, 2, 0.1)
        self.assertTrue(result["accepted"])
        self.assertGreater(result["score"], math.tan(0.1 / 4) ** 2)
        corners = np.array(
            [
                model.stored_normal(0, s, u, 0.1)
                for s, u in ((0, -2), (3, -2), (0, 2), (3, 2))
            ]
        )
        for a, b in np.random.default_rng(1906).uniform(0, 1, (20, 2)):
            weights = (
                np.array([1 - a - b, a, b, 0])
                if a + b <= 1
                else np.array([0, 1 - b, 1 - a, a + b - 1])
            )
            actual = weights @ corners
            actual /= np.linalg.norm(actual)
            expected = oracle_normal(curve, 0, 3 * a, -2 + 4 * b)
            self.assertLessEqual(
                math.atan2(
                    np.linalg.norm(np.cross(actual, expected)), actual @ expected
                ),
                0.1,
            )

    def test_longitudinal_reuse_keeps_lateral_bounds_and_limit_changes_distinct(self):
        model = NormalModel(Curve(recipe(curvature=0.02, height=0.01)))
        left = model.ranges(0, 0, 1, -2, -1)
        right = model.ranges(0, 0, 1, 1, 2)
        self.assertNotEqual(left[0], right[0])
        self.assertEqual(left[1:], right[1:])
        self.assertEqual(model.work["range_requests"], 2)
        self.assertEqual(model.work["longitudinal_evaluations"], 1)
        loose = model.cone(0, 0, 1, -1, 1, 0.2)
        strict = model.cone(0, 0, 1, -1, 1, 0.01)
        self.assertTrue(loose["accepted"])
        self.assertFalse(strict["accepted"])
        self.assertEqual(model.cone(0, 0, 1, -1, 1, 0.2), loose)
        self.assertEqual(model.work["angle_evaluations"], 3)
        with self.assertRaisesRegex(ValueError, "source-span"):
            model.ranges(0, -1, 1, -2, -1)

    def test_constant_slope_bank_whole_domain_needs_no_normal_refinement(self):
        source = recipe()
        for row in (*source["seams"], source["intervals"][0]["middle"]):
            row[3] = 0.3 * row[0]
            row[7] = 0.3
            row[9] = 0.2
        model = NormalModel(Curve(source))
        self.assertTrue(model.cone(0, 0, 10, -2, 2, 0.01)["accepted"])

    def test_stored_normal_matches_independent_source_differentiation(self):
        curve = Curve(recipe(curvature=0.05, height=0.015, bank_rate=0.025))
        model = NormalModel(curve)
        for s, u in ((0.1, -2), (3, 0.7), (8, 2), (10, -1)):
            got = model.stored_normal(0, s, u, 0.02)
            np.testing.assert_allclose(
                got, oracle_normal(curve, 0, s, u), atol=4e-8, rtol=0
            )

    def test_cone_bounds_normalized_positive_triangle_interpolation(self):
        curve = Curve(recipe(curvature=0.05, height=0.015, bank_rate=0.025))
        model = NormalModel(curve)
        lo, hi, u0, u1 = 3.0, 3.05, 0.1, 0.15
        self.assertTrue(model.cone(0, lo, hi, u0, u1, 0.02)["accepted"])
        corners = np.array(
            [
                model.stored_normal(0, s, u, 0.02)
                for s, u in ((lo, u0), (hi, u0), (lo, u1), (hi, u1))
            ],
            float,
        )
        for a, b in np.random.default_rng(1888).uniform(0, 1, (32, 2)):
            weights = (
                np.array([1 - a - b, a, b, 0])
                if a + b <= 1
                else np.array([0, 1 - b, 1 - a, a + b - 1])
            )
            actual = weights @ corners
            actual /= np.linalg.norm(actual)
            reference = oracle_normal(curve, 0, lo + a * (hi - lo), u0 + b * (u1 - u0))
            angle = math.atan2(
                np.linalg.norm(np.cross(actual, reference)), actual @ reference
            )
            self.assertLessEqual(angle, 0.02)

    def test_whole_heading_and_side_variation_cannot_be_hidden_by_endpoint_probes(self):
        model = NormalModel(
            Curve(recipe(curvature=0.05, height=0.015, bank_rate=0.025))
        )
        self.assertFalse(model.cone(0, 0, 10, -2, 2, 0.02)["accepted"])
        with self.assertRaisesRegex(ValueError, "source-span"):
            model.cone(0, -1, 1, -2, 2, 0.02)
        with self.assertRaises(ValueError):
            model.cone(0, 0, 1, -2, 2, 0)
        with self.assertRaises(ValueError):
            NormalModel(Curve(recipe(curvature=0.6)))


if __name__ == "__main__":
    unittest.main()
