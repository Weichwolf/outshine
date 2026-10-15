"""Independent placement counterexamples, source scope and owned detailed templates."""

import unittest

import numpy as np
import shapely
from outshine_ref.buildings.interiors import Furnishing, access, layout, placed
from shapely.affinity import rotate, translate

DESK = Furnishing("workstation", 1.5, 1.15, 1.35, 0.8)
STORAGE = Furnishing("storage", 1.05, 0.4, 1.55, 0.75)


class PlacementControls(unittest.TestCase):
    def test_motion_reservation_excludes_activity_without_blocking_transit(self):
        room = shapely.box(0, 0, 8, 6)
        item = placed(STORAGE, [4.0, 2.0], [1.0, 0.0], "motion-item")
        activity_only = shapely.box(3.6, 2.5, 4.4, 2.8)
        self.assertFalse(item["footprint"].intersects(activity_only))
        access(room, [(0.5, 3.0)], [item])
        with self.assertRaisesRegex(ValueError, "reserved movement"):
            access(room, [(0.5, 3.0)], [item], placement_reserved=(activity_only,))
        transit = shapely.box(2.0, 2.5, 2.4, 3.5)
        result = access(room, [(0.5, 3.0)], [item], placement_reserved=(transit,))
        self.assertTrue(any(path.intersects(transit) for path in result["paths"]))

    def test_reserved_motion_is_respected_before_complete_or_partial_placement(self):
        room = shapely.box(0, 0, 8, 6)
        reserved = shapely.box(2.0, 0.0, 5.0, 2.0)
        args = (room, [(0.5, 3.0)], (DESK, STORAGE), "motion-layout")
        result = layout(*args, placement_reserved=(reserved,))
        self.assertEqual(result["status"], "complete")
        self.assertEqual(len(result["items"]), 2)
        for item in result["items"]:
            for field in ("footprint", "activity"):
                self.assertLessEqual(item[field].intersection(reserved).area, 1e-9)
        blocked = layout(*args, placement_reserved=(room,))
        self.assertEqual(blocked["status"], "incomplete")
        self.assertEqual(blocked["items"], ())
        self.assertEqual(blocked["missing"], (DESK.kind, STORAGE.kind))

    def test_motion_reservations_reject_invalid_geometry_and_excess_capacity(self):
        room = shapely.box(0, 0, 8, 6)
        for regions in ((shapely.Point(1, 1),), (room,) * 17):
            with self.assertRaises(ValueError):
                access(room, [(0.5, 3.0)], [], placement_reserved=regions)

    def test_access_report_owns_portal_and_activity_coordinates(self):
        room = shapely.box(0, 0, 8, 6)
        portal = np.array([0.5, 3.0])
        item = placed(DESK, [4.0, 2.0], [1.0, 0.0], "caller-owned-target")
        target = item["targets"][0].copy()
        item = dict(item, targets=(target,))
        expected_target = target.copy()
        proof = access(room, [portal], [item])
        portal[:] = 99.0
        target[:] = 88.0
        np.testing.assert_array_equal(proof["anchors"][0], [0.5, 3.0])
        np.testing.assert_array_equal(proof["anchors"][1], expected_target)
        for anchor in proof["anchors"]:
            with self.assertRaises(ValueError):
                anchor.setflags(write=True)
        for path in proof["paths"]:
            self.assertTrue(proof["free"].covers(path))

    def test_placement_arrays_own_immutable_coordinates(self):
        centre = np.array([3.0, 2.0])
        tangent = np.array([1.0, 0.0])
        item = placed(DESK, centre, tangent, "immutable")
        centre[:] = 40.0
        tangent[:] = 20.0
        np.testing.assert_array_equal(item["centre"], [3.0, 2.0])
        np.testing.assert_array_equal(item["tangent"], [1.0, 0.0])
        for values in (
            item["centre"],
            item["tangent"],
            item["target"],
            *item["targets"],
        ):
            with self.assertRaises(ValueError):
                values.setflags(write=True)

    def test_hole_and_multiple_actual_portals_remain_connected(self):
        room = shapely.Polygon(
            [(0, 0), (10, 0), (10, 8), (0, 8)], [[(4, 3), (4, 5), (6, 5), (6, 3)]]
        )
        result = layout(
            room,
            [(0.5, 4.0), (9.5, 4.0)],
            (DESK, STORAGE),
            "hole",
            method="jittered-anchors",
            beam_width=4,
        )
        self.assertEqual(result["status"], "complete")
        self.assertGreater(len(result["proof"]["paths"]), 1)
        for item in result["items"]:
            self.assertTrue(room.covers(item["footprint"]))
            self.assertTrue(room.covers(item["activity"]))
        for path in result["proof"]["paths"]:
            self.assertTrue(result["proof"]["free"].covers(path))

    def test_invalid_inputs_and_hard_capacities_are_rejected(self):
        room = shapely.box(0, 0, 8, 6)
        for spec in (Furnishing,):
            with self.assertRaises(ValueError):
                spec("bad", True, 1.0, 1.0, 1.0)
        for portals in ([], [(float("nan"), 1)], [(1, 1, 1)], [(0.5, 3)] * 17):
            with self.assertRaises(ValueError):
                access(room, portals, [])
        with self.assertRaises(ValueError):
            layout(room, [(0.5, 3)], [DESK] * 33, "over-capacity")
        for options in (
            {"max_candidates": True},
            {"max_candidates": 257},
            {"method": "unbounded"},
            {"objective": "unknown"},
        ):
            with self.assertRaises(ValueError):
                layout(room, [(0.5, 3)], (DESK,), "invalid", **options)

    def test_concave_room_does_not_accept_four_inside_corners_over_a_notch(self):
        room = shapely.Polygon(
            [(0, 0), (6, 0), (6, 6), (4, 6), (4, 2), (2, 2), (2, 6), (0, 6)]
        )
        spec = Furnishing("wide", 5.0, 1.0, 1.0, 0.8)
        item = placed(spec, [3.0, 3.0], [1.0, 0.0], "notch")
        self.assertTrue(
            all(
                room.covers(shapely.Point(p))
                for p in list(item["footprint"].exterior.coords)[:-1]
            )
        )
        with self.assertRaises(ValueError):
            access(room, [(0.5, 0.5)], [item])

    def test_transformed_actual_placements_keep_containment_and_connected_targets(self):
        for angle in (0.0, 31.0, 95.0):
            room = translate(
                rotate(shapely.box(0, 0, 8, 6), angle, origin=(0, 0)), 112.0, -418.0
            )
            portal = translate(
                rotate(shapely.Point(0.5, 3.0), angle, origin=(0, 0)), 112.0, -418.0
            )
            result = layout(
                room,
                [next(iter(portal.coords))],
                (DESK, STORAGE),
                "rotated",
                method="jittered-anchors",
                beam_width=4,
            )
            self.assertEqual(result["status"], "complete")
            for item in result["items"]:
                self.assertTrue(room.covers(item["activity"]))
            for path in result["proof"]["paths"]:
                self.assertTrue(result["proof"]["free"].covers(path))

    def test_replay_limits_and_inputs(self):
        room = shapely.box(0, 0, 8, 6)
        args = (room, [(0.5, 3.0)], (DESK, STORAGE), "replay")
        a = layout(*args, method="jittered-anchors", beam_width=4)
        b = layout(*args, method="jittered-anchors", beam_width=4)
        self.assertEqual(a["status"], b["status"])
        self.assertEqual(a["tested"], b["tested"])
        for x, y in zip(a["items"], b["items"], strict=True):
            np.testing.assert_array_equal(x["centre"], y["centre"])
        for width in (0, 9, True):
            with self.assertRaises(ValueError):
                layout(*args, beam_width=width)
        with self.assertRaises(ValueError):
            Furnishing("bad", 1.0, float("nan"), 1.0, 1.0)


if __name__ == "__main__":
    unittest.main()
