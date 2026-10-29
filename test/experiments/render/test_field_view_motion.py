"""Dense replay binds one field, preserves handoffs and bounds caller paths."""

import unittest
from unittest.mock import patch

import numpy as np
from field_view_integrals import summed_field
from field_view_sampling import FieldGrid
from replay_field_view_motion import aligned, replay


class MotionTests(unittest.TestCase):
    def setUp(self):
        self.atlas = {"grid": FieldGrid((0, 0, 0), (0, 0, 1), 100)}
        self.source = np.ones((3, 3, 4), dtype=np.float32)
        self.declaration = {
            "source_binding": "complete-original",
            "radiance_binding": "lit-field",
            "samples_per_second": 60,
            "complete": True,
            "depth_separated": True,
            "composition_qualified": True,
            "camera": {"basis": [[1, 0, 0], [0, 1, 0], [0, 0, -1]], "focal_px": 900},
            "domain": {
                "distance_m": [60000, 68000],
                "angle_deg": 0.5,
                "measured_errors": [0.04, 0.02],
            },
            "knots": [
                {"time_s": 0, "eye": [0, 0, 60500]},
                {"time_s": 0.5, "eye": [0, 0, 62000]},
                {"time_s": 1, "eye": [0, 0, 59000]},
            ],
        }

    def test_one_prefix_and_exact_adjacent_enter_leave_frames(self):
        with patch(
            "replay_field_view_motion.summed_field", wraps=summed_field
        ) as build:
            result = replay(self.atlas, self.source, self.declaration)
        self.assertEqual(build.call_count, 1)
        self.assertEqual(result["samples"], 61)
        self.assertEqual(result["field_frames"] + result["parent_frames"], 61)
        self.assertEqual(len(result["transitions"]), 2)
        for transition in result["transitions"]:
            self.assertEqual(
                transition["after"]["index"] - transition["before"]["index"], 1
            )
            self.assertNotEqual(
                transition["after"]["mode"], transition["before"]["mode"]
            )

    def test_nonemissive_black_field_has_finite_motion_diagnostic(self):
        self.source[:, :, :3] = 0
        result = replay(self.atlas, self.source, self.declaration)
        self.assertEqual(result["maximum_held_field_rgb_relative_l1_step"], 0)

    def test_bad_time_and_sample_capacity_fail_before_table_build(self):
        for times in ([1, 0, 2], [0, 1, 2000]):
            for knot, time in zip(self.declaration["knots"], times, strict=True):
                knot["time_s"] = time
            with (
                patch(
                    "replay_field_view_motion.summed_field",
                    side_effect=AssertionError("allocation"),
                ),
                self.assertRaises(ValueError),
            ):
                replay(self.atlas, self.source, self.declaration)

    def test_origin_alignment_keeps_empty_gap_and_both_signals(self):
        previous = {"origin": (3, 4), "rgba": np.ones((1, 1, 4))}
        current = {"origin": (5, 4), "rgba": np.ones((1, 1, 4)) * 2}
        a, b = aligned(previous, current)
        np.testing.assert_array_equal(a[:, 1], 0)
        np.testing.assert_array_equal(b[:, 1], 0)
        self.assertEqual(a.sum(), 4)
        self.assertEqual(b.sum(), 8)


if __name__ == "__main__":
    unittest.main()
