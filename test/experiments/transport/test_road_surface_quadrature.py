"""Independent closed helix controls for source-span integration qualification."""

import math
import unittest
from fractions import Fraction as F

import numpy as np
from road_surface_lod import prepare, prepare_uniform
from road_surface_model import Curve, quadrature_remainder


def helix(length, curvature=0.1):
    def pose(station):
        angle = curvature * station
        return [
            station,
            math.sin(angle) / curvature,
            (1 - math.cos(angle)) / curvature,
            0.1 * station,
            angle,
            curvature,
            0.0,
            0.1,
            0.0,
            0.0,
            0.0,
        ]

    return {
        "range_m": [0.0, length],
        "section_m": [2.0, 0.0, 0.2],
        "seams": [pose(0), pose(length)],
        "intervals": [
            {
                "range_m": [0.0, length],
                "middle": pose(length / 2),
                "max_abs_curvature_per_m": abs(curvature),
            }
        ],
    }


def exact_vertices(stations, curvature):
    angles = curvature * stations
    centers = np.c_[
        np.sin(angles) / curvature,
        (1 - np.cos(angles)) / curvature,
        0.1 * stations,
    ]
    normal = np.c_[-np.sin(angles), np.cos(angles), np.zeros(len(stations))]
    return centers[:, None] + np.array([-2, -2, 2, 2])[None, :, None] * normal[:, None]


class SourceQuadratureTests(unittest.TestCase):
    def test_long_helix_declines_before_render_subdivision_in_both_methods(self):
        for method in (prepare, prepare_uniform):
            curve = Curve(helix(1000.0))
            self.assertGreater(curve.evaluation_error_bound_m, 0.25)
            with self.assertRaisesRegex(ValueError, "quadrature"):
                method(curve, 0.25)
            self.assertEqual(curve.evaluations, 0)

    def test_qualified_closed_helices_obey_the_stored_bound_at_every_vertex(self):
        for length in (10.0, 100.0, 300.0):
            for curvature in (0.1, -0.1):
                for method in (prepare, prepare_uniform):
                    with self.subTest(
                        length=length, curvature=curvature, method=method
                    ):
                        curve = Curve(helix(length, curvature))
                        plan = method(curve, 0.25)
                        truth = exact_vertices(plan["stations_m"], curvature)
                        error = np.linalg.norm(plan["positions_enh_m"] - truth, axis=2)
                        self.assertLessEqual(
                            error.max(), plan["maximum_stored_bound_m"]
                        )
                        self.assertLessEqual(plan["maximum_stored_bound_m"], 0.25)
                        self.assertEqual(
                            plan["evaluation_error_bound_m"],
                            curve.evaluation_error_bound_m,
                        )
                        self.assertGreaterEqual(
                            plan["maximum_model_bound_m"],
                            curve.evaluation_error_bound_m,
                        )

    def test_qualification_uses_the_requested_budget_not_a_fixed_phase_limit(self):
        for method in (prepare, prepare_uniform):
            curve = Curve(helix(300.0))
            self.assertGreater(curve.evaluation_error_bound_m, 1e-6)
            with self.assertRaisesRegex(ValueError, "quadrature"):
                method(curve, 1e-6)
            self.assertLessEqual(method(curve, 0.25)["maximum_stored_bound_m"], 0.25)

    def test_source_endpoint_disagreement_is_part_of_the_same_error_budget(self):
        source = helix(10.0)
        source["seams"][-1][1] += 0.4
        for method in (prepare, prepare_uniform):
            curve = Curve(source)
            self.assertGreater(curve.endpoint_disagreement_m, 0.39)
            with self.assertRaisesRegex(ValueError, "quadrature/pose"):
                method(curve, 0.25)

    def test_positive_recurrence_majorizes_the_independent_exact_derivative_sum(self):
        constant = F(math.factorial(16) ** 4, 33 * math.factorial(32) ** 3)
        for length, curvature, rate in (
            (10.0, 0.0, 0.0),
            (100.0, 0.1, 0.0),
            (300.0, 0.1, 0.0),
            (20.0, 0.4, 0.02),
            (20.0, 0.4, -0.02),
            (1.0, 1e-200, 0.0),
        ):
            a, b = F(curvature) * F(length), abs(F(rate)) * F(length) ** 2
            derivative = sum(
                F(
                    math.factorial(32),
                    2**j * math.factorial(j) * math.factorial(32 - 2 * j),
                )
                * a ** (32 - 2 * j)
                * b**j
                for j in range(17)
            )
            expected = F(length) * constant * derivative
            result = quadrature_remainder(length, curvature, rate)
            self.assertGreaterEqual(F(result), expected)
        self.assertEqual(quadrature_remainder(1e308, 1e308, 0.0), math.inf)


if __name__ == "__main__":
    unittest.main()
