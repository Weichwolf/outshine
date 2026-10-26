import math
import random
import struct
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
from road_station_sampling import HEADER, MARKER, captured, fill_gaps, original


class RoadStationSampling(unittest.TestCase):
    def test_dense_events_need_no_second_grid(self):
        events = [0, 1.5, 4.5, 7.5, 10]
        actual = fill_gaps(0, 10, 3, events)
        np.testing.assert_array_equal(actual, events)
        self.assertGreater(len(original(0, 10, 3, events)), len(actual))

    def test_exact_interval_lower_bound_and_events(self):
        rng = random.Random(915)
        for _ in range(1000):
            events = sorted(rng.choices(range(1, 400), k=rng.randrange(40)))
            events = [x / 4 for x in events]
            step = rng.choice([0.25, 0.5, 1, 2, 4])
            actual = fill_gaps(0, 100, step, events)
            anchors = sorted({0, *events, 100})
            bound = 1 + sum(
                math.ceil(
                    (Fraction(anchors[i + 1]) - Fraction(anchors[i])) / Fraction(step)
                )
                for i in range(len(anchors) - 1)
            )
            self.assertEqual(len(actual), bound)
            self.assertLessEqual(len(actual), len(original(0, 100, step, events)))
            self.assertTrue(np.isin(anchors, actual).all())
            self.assertTrue((np.diff(actual) > 0).all())
            self.assertLessEqual(max(np.diff(actual)), step)

    def test_unconstrained_uniform_positions_remain(self):
        np.testing.assert_array_equal(fill_gaps(0, 10, 3, []), [0, 3, 6, 9, 10])
        np.testing.assert_array_equal(fill_gaps(4, 10, 3, []), [4, 7, 10])

    def test_invalid_and_unrepresentable_inputs_do_not_loop(self):
        for events in ([4, 3], [-1], [11], [float("nan")]):
            with self.subTest(events=events), self.assertRaises(ValueError):
                fill_gaps(0, 10, 3, events)
        for step in (0, -1, float("nan"), float("inf"), 1e-300):
            with self.subTest(step=step), self.assertRaises(ValueError):
                fill_gaps(0, 10, step, [])
        with self.assertRaises(ValueError):
            fill_gaps(1e16, 1e16 + 2, 0.1, [])

    def test_capture_rejects_truncation_bad_marker_and_excessive_count(self):
        good = HEADER.pack(MARKER, 2, 0, 10, 3) + struct.pack("<dd", 1.5, 4.5)
        bad = [
            good[:-1],
            good[:10],
            HEADER.pack(0, 0, 0, 10, 3),
            HEADER.pack(MARKER, 262145, 0, 10, 3),
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "samples.bin"
            path.write_bytes(good)
            record = next(captured(path))
            np.testing.assert_array_equal(record[-1], [1.5, 4.5])
            for data in bad:
                path.write_bytes(data)
                with self.subTest(length=len(data)), self.assertRaises(ValueError):
                    list(captured(path))


if __name__ == "__main__":
    unittest.main()
