"""Real second free-soil refinement, unchanged contacts and bounded parent mapping."""

import unittest

import numpy as np
import shapely
from stair_blend_audit import inspect
from stair_blend_plan import prepare
from stair_blend_solve import candidate, validate_candidate
from stair_cycle_inputs import load
from stair_terrain_solid import retaining_body
from stair_west_bridge_checks import WEST, ActualWesternBridges


class WesternTerrain(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ActualWesternBridges.setUpClass()
        cls.data = ActualWesternBridges.data
        _, cls.rows, _, _, cls.context, sample = load(WEST)
        cls.sample = staticmethod(sample)
        result = prepare(
            cls.data,
            cls.rows,
            cls.context,
            None,
            cls.sample,
            precision="selective",
            interface="boundary_band",
            variables="fixed_displacement",
            sampling="transport_neighborhood",
            refinement_rounds=2,
        )
        cls.initial, cls.plan, cls.ground, cls.upper, cls.footprint, cls.note = result
        cls.heights, model, cls.admission = candidate(
            cls.plan, cls.upper, cls.footprint
        )
        validate_candidate(cls.plan, cls.heights, model, cls.admission)
        if not cls.admission["accepted_field"]:
            raise ValueError(str(cls.admission))
        cls.body, cls.parts = retaining_body(dict(cls.plan, targets=cls.heights))
        cls.proof = inspect(cls.body, cls.parts, cls.ground, cls.upper)

    def test_two_explicit_rounds_preserve_the_original_parent_and_ground_facets(self):
        self.assertEqual(len(self.note["refinement_passes"]), 2)
        self.assertGreater(
            self.note["refinement_passes"][1]["refinement"]["refined_cells"], 0
        )
        parents = self.plan["parent_faces"]
        self.assertGreaterEqual(int(parents.min()), 0)
        self.assertLess(int(parents.max()), len(self.initial["faces"]))
        original = shapely.polygons(self.initial["xy"][self.initial["faces"]])
        children = shapely.polygons(self.plan["xy"][self.plan["faces"]])
        self.assertTrue(
            shapely.covers(shapely.buffer(original[parents], 1e-7), children).all()
        )
        for key in ("faces", "targets"):
            before = (
                self.initial[key][self.initial["ground"]]
                if key == "faces"
                else self.initial[key]
            )
            after = (
                self.plan[key][self.plan["ground"]]
                if key == "faces"
                else self.plan[key][: len(before)]
            )
            np.testing.assert_array_equal(before, after)
        np.testing.assert_array_equal(
            self.initial["xy"], self.plan["xy"][: len(self.initial["xy"])]
        )

    def test_actual_whole_solid_and_both_precision_classes_keep_the_existing_limits(
        self,
    ):
        self.assertLessEqual(len(self.plan["xy"]), 16384)
        self.assertLessEqual(len(self.plan["faces"]), 32768)
        self.assertLessEqual(
            max(self.admission["maximum_soil_grade_double_float32"]), 2 / 3
        )
        self.assertLessEqual(
            max(self.admission["maximum_support_grade_double_float32"]), 10
        )
        for result in self.proof["bridge_clearance"].values():
            self.assertGreaterEqual(result["minimum_clearance_m"], 0)
        self.assertEqual(self.proof["interior_height_jumps"], 0)

    def test_refinement_is_explicit_and_not_an_unbounded_retry(self):
        for rounds in (0, 3, True, 1.0):
            with (
                self.subTest(rounds=rounds),
                self.assertRaisesRegex(ValueError, "one or two"),
            ):
                prepare(None, None, None, None, None, refinement_rounds=rounds)


if __name__ == "__main__":
    unittest.main()
