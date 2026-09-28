#!/usr/bin/env python3
"""Analytic paths and deliberately invalid captures for the motion oracle."""

import csv
import math
import pathlib
import tempfile
import unittest
from dataclasses import replace

from motion_trace import Limits, evaluate


class MotionTrace(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = pathlib.Path(self.folder.name) / "motion.tsv"
        self.limits = Limits(4.0, 1.0, 2.0, 0.02, 2.0)

    def rows(self):
        rows = []
        for tick in range(1, 5):
            angle = 0.01 * tick
            rows.append({"time_s": tick, "station_m": tick, "camera_frame_serial": tick,
                         "camera_eye_east_m": tick, "camera_eye_up_m": 2,
                         "camera_eye_south_m": 0, "camera_forward_east": math.sin(angle),
                         "camera_forward_up": 0, "camera_forward_south": -math.cos(angle),
                         "camera_up_east": 0, "camera_up_up": 1, "camera_up_south": 0,
                         "left_contact": 1, "center_contact": 1, "right_contact": 1,
                         "eye_clearance_m": 1.5})
        return rows

    def check(self, rows, limits=None):
        with self.path.open("w", newline="") as target:
            writer = csv.DictWriter(target, fieldnames=list(rows[0]), delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
        return evaluate(self.path, limits or self.limits)

    def test_analytic_translation_and_rotation(self):
        result = self.check(self.rows())
        self.assertAlmostEqual(result["maximum_position_step_m"], 1)
        self.assertAlmostEqual(result["maximum_rotation_step_rad"], 0.01)
        self.assertAlmostEqual(result["closure_rotation_rad"], 0.03)

    def test_position_jump(self):
        rows = self.rows()
        rows[2]["camera_eye_east_m"] = 100
        with self.assertRaisesRegex(ValueError, "step exceeds budget"):
            self.check(rows)

    def test_yaw_and_roll_jumps(self):
        for roll in (False, True):
            rows = self.rows()
            if roll:
                rows[2]["camera_up_up"] = -1
            else:
                rows[2]["camera_forward_east"] *= -1
                rows[2]["camera_forward_south"] *= -1
            with self.subTest(roll=roll), self.assertRaisesRegex(ValueError, "step exceeds budget"):
                self.check(rows)

    def test_yaw_wrap_is_not_a_jump(self):
        rows = self.rows()
        for row, angle in zip(rows, (3.13, 3.14, -3.133185307179586, -3.123185307179586)):
            row["camera_forward_east"] = math.sin(angle)
            row["camera_forward_south"] = -math.cos(angle)
        self.assertAlmostEqual(self.check(rows)["maximum_rotation_step_rad"], 0.01)

    def test_quantized_identical_basis_has_zero_rotation(self):
        rows = self.rows()
        for row in rows:
            row["camera_forward_east"] = 0.707106781
            row["camera_forward_south"] = -0.707106781
        result = self.check(rows, replace(self.limits, rotation_step_rad=1e-9))
        self.assertEqual(result["maximum_rotation_step_rad"], 0)

    def test_closed_position_and_orientation_are_independent(self):
        rows = self.rows()
        for row in rows:
            row["camera_eye_east_m"] = 0
        limits = replace(self.limits, closure_position_m=0, closure_rotation_rad=0.04, lap_length_m=4.0)
        self.check(rows, limits)
        with self.assertRaisesRegex(ValueError, "does not close"):
            self.check(rows, replace(limits, closure_rotation_rad=0.001))
        rows[-1]["camera_eye_east_m"] = 0.1
        with self.assertRaisesRegex(ValueError, "does not close"):
            self.check(rows, limits)

    def test_stale_serial_tick_gap_contact_and_invalid_basis(self):
        faults = {"camera_frame_serial": 1, "time_s": 1, "station_m": 0,
                  "left_contact": 0, "center_contact": 0, "right_contact": 0,
                  "camera_up_up": 2, "camera_forward_up": 1,
                  "camera_eye_up_m": float("nan"), "eye_clearance_m": -0.1}
        for column, value in faults.items():
            rows = self.rows()
            rows[2][column] = value
            with self.subTest(column=column), self.assertRaises(ValueError):
                self.check(rows)

    def test_station_jump_and_unfinished_lap(self):
        rows = self.rows()
        rows[2]["station_m"] = 100
        with self.assertRaisesRegex(ValueError, "station step"):
            self.check(rows)
        rows = self.rows()
        for row in rows:
            row["camera_eye_east_m"] = 0
        limits = replace(self.limits, closure_position_m=0, closure_rotation_rad=0.04,
                         lap_length_m=5.0)
        with self.assertRaisesRegex(ValueError, "not completed"):
            self.check(rows, limits)

    def test_missing_camera_columns_and_truncation_are_not_evidence(self):
        rows = self.rows()
        with self.assertRaisesRegex(ValueError, "incomplete trace"):
            self.check(rows[:-1])
        for row in rows:
            del row["camera_up_south"]
        with self.assertRaisesRegex(ValueError, "missing/invalid"):
            self.check(rows)

    def test_invalid_budgets(self):
        for limits in (replace(self.limits, step_s=0),
                       replace(self.limits, rotation_step_rad=float("nan")),
                       replace(self.limits, duration_s=4.5),
                       replace(self.limits, closure_position_m=0.1)):
            with self.assertRaises(ValueError):
                self.check(self.rows(), limits)


if __name__ == "__main__":
    unittest.main()
