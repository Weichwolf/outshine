"""Buffered geometric traces are not extended again in station space."""

import json
import unittest
from pathlib import Path

import numpy as np
from stair_module_trace import agree


class WholeTraceIntervals(unittest.TestCase):
    def test_a_sub_guard_station_gap_is_still_missing_actual_trace(self):
        whole = np.array([[0, 1, 100, 100]], float)
        gap = np.array([[0, 0.49, 100, 100], [0.51, 1, 100, 100]], float)
        with self.assertRaisesRegex(ValueError, "uncovered interval"):
            agree(whole, gap, 1, xy_guard=0.02, z_guard=1e-7)

    def test_captured_wien_trace_keeps_its_existing_spatial_and_height_guards(self):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/profile-rounded-trace-intervals.json"
        )
        saved = json.loads(path.read_text())
        a, b = [np.array(p) for p in saved["pieces"]]
        line = np.array(saved["line"])
        proof = agree(
            a,
            b,
            np.linalg.norm(line[-1] - line[0]),
            xy_guard=saved["bounds"]["xy_guard_m"],
            z_guard=saved["bounds"]["z_guard_m"],
        )
        self.assertLessEqual(
            proof["maximum_height_disagreement_m"], saved["bounds"]["z_guard_m"]
        )
        b[:, 2:] += 1e-4
        with self.assertRaisesRegex(ValueError, "heights disagree"):
            agree(
                a,
                b,
                np.linalg.norm(line[-1] - line[0]),
                xy_guard=saved["bounds"]["xy_guard_m"],
                z_guard=saved["bounds"]["z_guard_m"],
            )


if __name__ == "__main__":
    unittest.main()
