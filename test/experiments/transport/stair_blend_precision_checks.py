"""Real skinny-facet rounding failure and independent admission of guarded fits."""

import unittest

import numpy as np
import shapely
from stair_blend_precision import guard_data
from stair_blend_solve import check_heights, solve


def actual_skinny_facet():
    xy = np.array(
        [
            [0.6523538664265769, -5.292325647905762],
            [-5.0545705436553465, -5.293842069441855],
            [-3.150996585832324, -5.294347543287219],
            [-20, -10],
            [20, -10],
            [20, 10],
            [-20, 10],
        ]
    )
    targets = np.array(
        [165.7787991635501, 164.96518035871907, 165.23722357521544, 166, 166, 166, 166]
    )
    bed = np.full(len(xy), np.nan)
    bed[0] = targets[0]
    return {
        "xy": xy,
        "faces": np.array([[0, 1, 2]]),
        "ground": np.array([False]),
        "targets": targets,
        "bed": bed,
    }


class QuantizedTerrain(unittest.TestCase):
    def test_actual_double_valid_facet_is_rejected_after_float32_rounding(self):
        layout = actual_skinny_facet()
        model = {"soil_faces": layout["faces"], "eligible": np.array([False])}
        with self.assertRaisesRegex(ValueError, "escapes the declared contact zone"):
            check_heights(layout, layout["targets"], model)

    def test_both_guard_strategies_fix_the_real_facet_without_raising_soil_limit(self):
        for mode in ("selective", "all"):
            layout = dict(actual_skinny_facet(), precision_mode=np.array(mode))
            heights, _roles, note = solve(layout, None, shapely.box(100, 100, 110, 110))
            self.assertIsNotNone(heights, note)
            self.assertEqual(heights[0], layout["bed"][0])
            self.assertLessEqual(note["maximum_soil_grade_double_float32"][1], 2 / 3)
            self.assertEqual(note["constructed_faces"], 0)
            self.assertEqual(note["precision"]["additional_gradient_cones"], 1)

    def test_guarded_field_cannot_leave_its_declared_height_interval(self):
        layout = dict(actual_skinny_facet(), precision_mode=np.array("selective"))
        heights = layout["targets"] + 100
        with self.assertRaisesRegex(ValueError, "declared precision height interval"):
            check_heights(layout, heights, {"soil_faces": layout["faces"]})

    def test_safe_well_conditioned_facets_skip_only_provably_unneeded_cones(self):
        layout = actual_skinny_facet()
        layout["xy"][:3] = [[0, 0], [1, 0], [0, 1]]
        layout["precision_mode"] = np.array("selective")
        _weights, selected, _error, _bounds, _epsilon = guard_data(
            layout, layout["faces"], np.array([False]), 10
        )
        self.assertEqual(len(selected), 0)


if __name__ == "__main__":
    unittest.main()
