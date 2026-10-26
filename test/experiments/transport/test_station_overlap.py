"""Independent elevations retain every geometric subface after real station cuts."""

import copy
import unittest

import numpy as np
from mvt_t_stair_loft import prepare_stair_lofts
from scipy.optimize import linprog
from stair_junction_solve import Conditions, overlap_conditions
from stair_planar_lineage import overlap_faces
from station_overlap_case import prepared


class StationOverlapTests(unittest.TestCase):
    def test_actual_cut_keeps_all_subfaces_and_independent_height_variables(self):
        for angle, translation, permute in (
            (0, (0, 0), False),
            (0.437, (37, -81), True),
            (-0.83, (100000, -120000), True),
        ):
            with self.subTest(angle=angle, translation=translation, permute=permute):
                plan, _contacts = prepared(angle, translation, permute=permute)
                before = overlap_conditions(plan, Conditions(len(plan["xy"])), 0)
                original_xy = plan["xy"].copy()
                rows = copy.deepcopy(plan["rows"])
                prepare_stair_lofts(plan)
                self.assertGreater(before, 0)
                self.assertGreater(
                    overlap_conditions(plan, Conditions(len(plan["xy"])), 0), before
                )
                np.testing.assert_array_equal(
                    plan["xy"][: len(original_xy)], original_xy
                )
                self.assertEqual(
                    [r["source_id"] for r in rows],
                    [r["source_id"] for r in plan["rows"]],
                )
                owner = plan["stairs"][0][0]
                low, high = plan["parts"][owner], plan["parts"][-1]
                self.assertFalse(set(low.ravel()) & set(high.ravel()))
                self.assertEqual(len(low), len(set(plan["part_global_faces"][owner])))
                mapped = overlap_faces(plan)
                self.assertEqual(mapped[owner].keys(), mapped[-1].keys())
                self.assertEqual(len(mapped[owner]), len(low))
                for key in mapped[owner]:
                    np.testing.assert_array_equal(
                        plan["xy"][mapped[owner][key]], plan["xy"][mapped[-1][key]]
                    )

    def test_unchanged_highs_accepts_separate_planes_and_rejects_inverted_heights(self):
        plan, _contacts = prepared()
        prepare_stair_lofts(plan)
        system = Conditions(len(plan["xy"]))
        self.assertGreater(overlap_conditions(plan, system, 2.82), 0)
        matrix = system.matrix()
        upper = np.unique(plan["parts"][-1])
        plane = plan["xy"] @ [0.01, -0.02]
        for separation, expected in ((3, True), (-3, False)):
            heights = plane.copy()
            heights[upper] += separation
            answer = linprog(
                np.zeros(len(heights)),
                A_ub=matrix,
                b_ub=system.bounds,
                bounds=list(zip(heights, heights, strict=True)),
                method="highs-ds",
            )
            self.assertEqual(answer.success, expected)
        plan["rows"][-1]["separation_below_source_ids"] = []
        for row in plan["rows"][:-1]:
            row["separation_below_source_ids"] = [plan["rows"][-1]["source_id"]]
        opposite = Conditions(len(heights))
        overlap_conditions(plan, opposite, 2.82)
        heights = plane.copy()
        heights[upper] += 3
        answer = linprog(
            np.zeros(len(heights)),
            A_ub=opposite.matrix(),
            b_ub=opposite.bounds,
            bounds=list(zip(heights, heights, strict=True)),
            method="highs-ds",
        )
        self.assertFalse(answer.success)

    def test_variable_gap_keeps_the_same_corner_pairs(self):
        plan, _contacts = prepared(permute=True)
        prepare_stair_lofts(plan)
        count = len(plan["xy"])
        fixed, variable = Conditions(count), Conditions(count + 1)
        checks = overlap_conditions(plan, fixed, 2.82)
        self.assertEqual(overlap_conditions(plan, variable, 0, count), checks)
        np.testing.assert_array_equal(
            variable.matrix()[:, :count].toarray(), fixed.matrix().toarray()
        )
        np.testing.assert_array_equal(
            variable.matrix()[:, count].toarray(), np.ones((checks, 1))
        )

    def test_parent_id_reuse_cannot_drop_children_or_zip_different_corners(self):
        plan = {
            "xy": np.array([[0, 0], [1, 0], [1, 1], [0, 1]] * 2, float),
            "parts": [
                np.array([[0, 1, 2], [0, 2, 3]]),
                np.array([[6, 7, 4], [5, 6, 4]]),
            ],
            "part_global_faces": [np.array([7, 7]), np.array([7, 7])],
            "rows": [
                {"source_id": "low", "original": {"bridge": False}},
                {"source_id": "high", "original": {"bridge": True}},
            ],
        }
        system = Conditions(8)
        self.assertEqual(overlap_conditions(plan, system, 2.82), 6)
        matrix = system.matrix()
        for z, expected in (([0] * 4 + [3] * 4, True), ([0] * 4 + [-3] * 4, False)):
            answer = linprog(
                np.zeros(8),
                A_ub=matrix,
                b_ub=system.bounds,
                bounds=list(zip(z, z, strict=True)),
                method="highs-ds",
            )
            self.assertEqual(answer.success, expected)
        plan["rows"][1]["original"]["bridge"] = False
        with self.assertRaisesRegex(ValueError, "unplanned non-contact"):
            overlap_conditions(plan, Conditions(8), 0)

    def test_mismatched_geometry_is_not_invented_from_parent_id(self):
        plan = {
            "xy": np.array([[0, 0], [1, 0], [0, 1], [10, 0], [11, 0], [10, 1]], float),
            "parts": [np.array([[0, 1, 2]]), np.array([[3, 4, 5]])],
            "part_global_faces": [np.array([0]), np.array([0])],
            "rows": [{}, {}],
        }
        self.assertEqual(overlap_conditions(plan, Conditions(6), 0), 0)

    def test_repeated_same_source_subface_is_explicitly_refused(self):
        plan = {
            "xy": np.array([[0, 0], [1, 0], [0, 1]], float),
            "parts": [np.array([[0, 1, 2], [1, 2, 0]])],
            "part_global_faces": [np.array([4, 4])],
            "rows": [{}],
        }
        with self.assertRaisesRegex(ValueError, "duplicate prepared planar subface"):
            overlap_conditions(plan, Conditions(3), 0)


if __name__ == "__main__":
    unittest.main()
