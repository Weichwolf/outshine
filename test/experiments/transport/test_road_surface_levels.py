import unittest

import numpy as np
from road_surface_levels import products
from road_surface_lod import prepare
from road_surface_model import Curve
from test_road_surface_lod import recipe


class RoadSurfaceLevels(unittest.TestCase):
    def test_a_consumer_cannot_mutate_the_next_product_through_its_buffers(self):
        chain = products(Curve(recipe()), (1.0, 0.25))
        coarse = next(chain)
        coarse["positions_enh_m"][:] = np.nan
        coarse["stations_m"][:] = np.nan
        coarse["indices"][:] = 0
        fine = next(chain)
        reference = prepare(Curve(recipe()), 0.25)
        for key in ("positions_enh_m", "stations_m", "indices"):
            np.testing.assert_array_equal(fine[key], reference[key])

    def test_matches_independent_products_at_every_requested_error(self):
        source = recipe(curvature=0.03, height=0.02, bank_rate=0.04, knots=7)
        errors = (1.0, 0.25, 0.0625)
        previous = None
        for plan, error in zip(products(Curve(source), errors), errors):
            reference = prepare(Curve(source), error)
            for key in ("stations_m", "positions_enh_m", "indices"):
                np.testing.assert_array_equal(plan[key], reference[key])
            self.assertEqual(
                plan["maximum_stored_bound_m"], reference["maximum_stored_bound_m"]
            )
            if previous is not None:
                self.assertTrue(np.isin(previous, plan["stations_m"]).all())
                self.assertGreater(plan["reused_intervals"], 0)
            previous = plan["stations_m"]

    def test_coarse_request_does_not_build_unrequested_fine_products(self):
        source = recipe(curvature=0.1, height=0.1)
        curve = Curve(source)
        chain = products(curve, (1.0, 0.00001))
        coarse = next(chain)
        count = curve.evaluations
        reference = prepare(Curve(source), 1.0)
        self.assertEqual(count, reference["pose_evaluations"])
        self.assertEqual(
            coarse["envelope_pose_tests"], reference["envelope_pose_tests"]
        )
        chain.close()
        self.assertEqual(curve.evaluations, count)

    def test_finer_budget_failure_keeps_the_already_published_coarse_product(self):
        chain = products(Curve(recipe(curvature=0.1)), (10.0, 0.00001), max_stations=2)
        coarse = next(chain)
        original = coarse["positions_enh_m"].copy()
        with self.assertRaises(RuntimeError):
            next(chain)
        np.testing.assert_array_equal(coarse["positions_enh_m"], original)
        self.assertEqual(len(coarse["stations_m"]), 2)

    def test_several_levels_reuse_queries_instead_of_rebuilding_the_tree(self):
        source = recipe(curvature=0.1, height=0.1, bank_rate=0.02)
        errors = (1.0, 0.25, 0.0625)
        independent = sum(
            prepare(Curve(source), error)["envelope_pose_tests"] for error in errors
        )
        plans = list(products(Curve(source), errors))
        self.assertLess(plans[-1]["cumulative_envelope_pose_tests"], independent)
        self.assertEqual(
            plans[-1]["cumulative_envelope_pose_tests"],
            prepare(Curve(source), errors[-1])["envelope_pose_tests"],
        )

    def test_invalid_demand_and_exhausted_cumulative_work_are_explicit(self):
        for errors in ((), (0,), (float("nan"),), (1, 1), (1, 2)):
            with self.subTest(errors=errors), self.assertRaises(ValueError):
                next(products(Curve(recipe()), errors))
        curve = Curve(recipe())
        with self.assertRaises(RuntimeError):
            next(products(curve, (1,), max_queries=1))
        self.assertEqual(curve.evaluations, 0)


if __name__ == "__main__":
    unittest.main()
