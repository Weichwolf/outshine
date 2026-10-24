"""Per-stamp ranges preserve actual body queries and native clamp semantics."""

import dataclasses
import struct
import unittest

import numpy as np
from earthwork_height_ranges import Stamp, direct, prepare
from earthwork_height_ranges_experiment import evaluate, probes, world_stamps


def words(value):
    return struct.pack("d", value)


class EarthworkHeightRanges(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.stamps = world_stamps()

    def test_actual_world_grid_candidates_are_byte_identical(self):
        ids, points = probes(self.stamps)
        old = {k: a.tobytes() for k, a in self.case["data"].items()}
        direct_values, _ = evaluate(self.stamps, ids, points, "direct")
        prepared_values, table = evaluate(self.stamps, ids, points, "prepared")
        self.assertEqual(direct_values.tobytes(), prepared_values.tobytes())
        self.assertEqual(len(self.stamps), 58)
        self.assertGreater(len(ids), 1000)
        for stamp, prepared in zip(self.stamps, table, strict=True):
            self.assertIs(prepared.stamp, stamp)
        for key, value in old.items():
            self.assertEqual(self.case["data"][key].tobytes(), value)

    def test_body_corners_centres_and_exterior_keep_each_cap_range(self):
        table = prepare(self.stamps)
        clamps = 0
        for stamp, held in zip(self.stamps, table, strict=True):
            for point in (
                *stamp.ring,
                tuple(np.mean(stamp.ring, axis=0)),
                (stamp.at_e + 20, stamp.at_n - 20),
            ):
                value = direct(stamp, point)
                self.assertEqual(words(value), words(held.at(point)))
                clamps += value != stamp.wants_at(point)
                self.assertGreaterEqual(value, held.limits[0])
                self.assertLessEqual(value, held.limits[1])
        self.assertGreater(clamps, 0)

    def test_non_corridor_zero_key_and_short_rings_remain_unclamped(self):
        base = Stamp(((0, 0), (1, 0), (0, 1)), 0, 0, 0, 2, 3)
        for stamp in (
            dataclasses.replace(base, kind="Pad"),
            dataclasses.replace(base, kind="Clearance"),
            dataclasses.replace(base, corridor_key=0),
            dataclasses.replace(base, ring=base.ring[:2]),
            dataclasses.replace(base, ring=()),
        ):
            (prepared,) = prepare([stamp])
            self.assertIsNone(prepared.limits)
            self.assertEqual(direct(stamp, (4, 5)), 23)
            self.assertEqual(prepared.at((4, 5)), 23)
        self.assertEqual(direct(base, (4, 5)), 3)

    def test_sag_and_native_sentinel_are_not_replaced_by_vertex_z_bounds(self):
        stamp = Stamp(((0, 0), (2, 0), (0, 2)), 0, 0, 10, 2, 3, sag_inv=1)
        (held,) = prepare([stamp])
        self.assertEqual(held.limits, (10, 14))
        self.assertEqual(held.at((1, 1)), 14)
        self.assertEqual(held.at((20, 20)), 10)
        huge = dataclasses.replace(stamp, plateau=1e31, slope_e=0, slope_n=0, sag_inv=0)
        (prepared,) = prepare([huge])
        self.assertEqual(prepared.limits, (1e30, 1e31))
        self.assertEqual(words(prepared.at((1, 2))), words(direct(huge, (1, 2))))

    def test_coplanar_merge_would_change_each_stamp_clamp(self):
        first = Stamp(((0, 0), (1, 0), (0, 1)), 0, 0, 0, 1, 0)
        second = dataclasses.replace(first, ring=((1, 0), (2, 0), (2, 1)))
        self.assertEqual(first.wants_at((3, 0)), second.wants_at((3, 0)))
        self.assertEqual(direct(first, (3, 0)), 1)
        self.assertEqual(direct(second, (3, 0)), 2)
        merged = dataclasses.replace(first, ring=first.ring + second.ring)
        self.assertNotEqual(direct(first, (3, 0)), direct(merged, (3, 0)))

    def test_snapshot_owns_immutable_ring_and_ranges_keep_stamp_identity(self):
        ring = [[0, 0], [1, 0], [0, 1]]
        origin = np.array(0.0)
        stamp = Stamp(ring, origin, 0, 0, 1, 0)
        (held,) = prepare([stamp])
        before = held.at((3, 0))
        ring[1][0] = 7
        origin[...] = 9
        self.assertEqual(held.at((3, 0)), before)
        self.assertIs(type(stamp.at_e), float)
        self.assertIs(held.stamp, stamp)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            stamp.plateau = 10
        with self.assertRaises(TypeError):
            stamp.ring[1][0] = 7

    def test_budget_and_cancellation_preserve_inputs_and_publish_no_partial_table(self):
        original = tuple(self.stamps)
        for kwargs in (
            {"max_stamps": 57},
            {"max_ring_points": 173},
            {"max_stamps": -1},
            {"max_ring_points": True},
        ):
            with self.assertRaises(ValueError):
                prepare(self.stamps, **kwargs)
        checks = iter([False, False, True])
        with self.assertRaisesRegex(ValueError, "cancelled"):
            prepare(self.stamps, cancelled=lambda: next(checks))
        with self.assertRaisesRegex(ValueError, "cancelled"):
            prepare([], cancelled=lambda: True)
        self.assertEqual(original, self.stamps)
        self.assertEqual(prepare([], max_stamps=0, max_ring_points=0), ())

    def test_nonfinite_and_overflowing_stamps_are_not_prepared(self):
        stamp = self.stamps[0]
        for kwargs in (
            {"plateau": float("nan")},
            {"ring": ((0, 0), (1, float("inf")), (0, 1))},
            {"corridor_key": 2**64},
        ):
            with self.assertRaises(ValueError):
                dataclasses.replace(stamp, **kwargs)
        huge = dataclasses.replace(stamp, slope_e=1e308, at_e=-1e308)
        with self.assertRaisesRegex(ValueError, "finite ring height"):
            prepare([huge])


if __name__ == "__main__":
    unittest.main()
