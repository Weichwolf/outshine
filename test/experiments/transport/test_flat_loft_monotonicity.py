"""Constant loft heights imply zero grade without subtracting inverse gradients."""

import unittest
from unittest.mock import patch

import numpy as np
import stair_junction_solve
from mvt_t_stair_loft import height_projection
from stair_junction_solve import Conditions, stair_condition


class FlatLoftMonotonicity(unittest.TestCase):
    def setUp(self):
        self.xy = np.array(
            [[0, 0], [1, 0], [1, 2**-30], [2, 0], [3, 0], [3, 2**-30]],
            dtype=float,
        )
        self.faces = np.array([[0, 1, 2], [1, 3, 2], [3, 4, 5]])
        self.loft = {
            "owner": 0,
            "start_node": 0,
            "end_node": 1,
            "start_cut_m": 1.0,
            "end_cut_m": 2.0,
            "stations_m": self.xy[:, 0].tolist(),
        }
        self.plan = {
            "xy": self.xy,
            "rows": [{"xy": np.array([[0.0, 0.0], [3.0, 0.0]]), "width_m": 1.0}],
            "parts": [self.faces],
            "stair_lofts": [self.loft],
        }

    def conditions(self, lofts=(), direction=1):
        system = Conditions(len(self.xy) + 6)
        with patch.object(stair_junction_solve, "cross_sections", return_value=[]):
            skipped = stair_condition(
                self.plan,
                system,
                0,
                0,
                1,
                plane_offset=len(self.xy),
                direction=direction,
                height_lofts=lofts,
            )
        return system, skipped

    def test_constant_caps_have_identical_height_expressions(self):
        projection = height_projection(self.plan, len(self.xy) + 6).toarray()
        for face, node in ((self.faces[0], 0), (self.faces[2], 1)):
            for vertex in face:
                np.testing.assert_array_equal(
                    projection[vertex], projection[len(self.xy) + 3 * node]
                )
        self.assertFalse(np.array_equal(projection[1], projection[3]))

    def test_only_sloped_face_needs_a_monotonic_gradient(self):
        actual = stair_junction_solve.gradient_rows
        for direction in (-1, 1):
            with self.subTest(direction=direction):
                with patch.object(
                    stair_junction_solve, "gradient_rows", wraps=actual
                ) as gradients:
                    system, skipped = self.conditions([self.loft], direction)
                self.assertEqual(skipped, 2)
                self.assertEqual(gradients.call_count, 1)
                np.testing.assert_array_equal(
                    gradients.call_args.args[1], self.faces[1]
                )
                self.assertEqual(len(system.bounds), 2 * len(self.xy) + 1)
                # A declining active flight must still violate its retained row.
                heights = -direction * self.xy[:, 0]
                solution = np.r_[heights, np.zeros(6)]
                self.assertGreater((system.matrix() @ solution)[-1], 0)

    def test_generic_and_foreign_owner_lofts_keep_all_monotonic_rows(self):
        for lofts in ((), [dict(self.loft, owner=1)]):
            system, skipped = self.conditions(lofts)
            self.assertEqual(skipped, 0)
            self.assertEqual(len(system.bounds), 2 * len(self.xy) + 3)

    def test_face_crossing_cut_is_kept_without_a_station_epsilon(self):
        self.loft["start_cut_m"] = np.nextafter(1.0, 0.0)
        self.loft["end_cut_m"] = np.nextafter(2.0, np.inf)
        system, skipped = self.conditions([self.loft])
        self.assertEqual(skipped, 0)
        self.assertEqual(len(system.bounds), 2 * len(self.xy) + 3)


if __name__ == "__main__":
    unittest.main()
