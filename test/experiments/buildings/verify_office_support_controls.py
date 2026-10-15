"""Counterexamples for whole support polygons, retained apertures and rigid frame changes."""

import unittest

import numpy as np
import shapely
from building_layout import rectangle
from office_support_geometry import MAX_CANDIDATES, candidates, split
from outshine_ref.buildings.interiors import layout
from room_programme import BASIN, TEA, WC
from shapely.affinity import rotate, translate


def fixture():
    door = {
        "id": "old",
        "rooms": [0, 1],
        "centre": [5.0, -0.06],
        "tangent": [1.0, 0.0],
        "polygon": rectangle([5.0, -0.06], [1.0, 0.0], 1.1, 0.2, 0.2),
        "height_m": 2.15,
        "kind": "corridor-door",
    }
    return {
        "rooms": [shapely.box(0, -2, 10, -0.12), shapely.box(0, 0, 10, 6)],
        "room_kinds": ["corridor", "generic-room"],
        "doors": [door],
        "windows": [],
    }


class SupportControls(unittest.TestCase):
    def test_compact_support_fits_full_functions_beside_reserved_door_approach(self):
        room = shapely.box(0.0, 0.0, 1.68, 3.54)
        approach = shapely.LineString([(0.84, -0.06), (0.84, 0.49)]).buffer(
            0.42, cap_style="square", join_style="mitre"
        )
        for required in ((WC, BASIN), (TEA,)):
            result = layout(
                room,
                [(0.84, 0.49)],
                required,
                "compact-support",
                [approach],
                "wall-anchors",
                beam_width=4,
                objective="open-activity",
            )
            self.assertEqual(result["status"], "complete")
            self.assertEqual(tuple(item["spec"] for item in result["items"]), required)
            for item in result["items"]:
                self.assertTrue(room.covers(item["footprint"]))
                self.assertFalse(item["footprint"].intersects(approach))

    def test_independent_corridor_access_preserves_old_door_and_whole_parent(self):
        original = fixture()
        for method in ("wall-ends", "sliding-wall"):
            accepted, rejected = candidates(original, 1, "sanitary", method)
            self.assertTrue(accepted)
            self.assertLessEqual(len(accepted) + len(rejected), MAX_CANDIDATES)
            for result in accepted:
                child = result["child"]
                self.assertTrue(original["rooms"][1].covers(child))
                self.assertLess(child.area, 7.0)
                self.assertGreater(child.area, 4.5)
                self.assertEqual(result["door"]["rooms"], [0, 2])
                centre = np.asarray(result["door"]["centre"])
                tangent = np.asarray(result["door"]["tangent"])
                normal = np.array([-tangent[1], tangent[0]])
                for sign, index in ((-1, 0), (1, 2)):
                    backing = shapely.LineString(
                        [
                            centre + normal * sign * 0.46 - tangent * 0.55,
                            centre + normal * sign * 0.46 + tangent * 0.55,
                        ]
                    )
                    self.assertTrue(result["rooms"][index].covers(backing))
                self.assertTrue(result["rooms"][1].covers(shapely.Point(5.0, 0.49)))
            self.assertTrue(
                original["rooms"][1].equals_exact(shapely.box(0, 0, 10, 6), 0)
            )

    def test_existing_door_approach_cannot_be_consumed_by_small_room(self):
        original = fixture()
        zone = shapely.box(3.5, 0, 6.5, 2.1)
        with self.assertRaisesRegex(ValueError, "existing door approach"):
            split(
                original,
                1,
                zone,
                shapely.LineString([(3.5, -0.06), (6.5, -0.06)]),
                "sanitary",
            )

    def test_notched_host_does_not_accept_corner_only_containment(self):
        original = fixture()
        original["rooms"][1] = original["rooms"][1].difference(
            shapely.box(0.5, 1, 1.1, 6.1)
        )
        accepted, _ = candidates(original, 1, "sanitary", "sliding-wall")
        self.assertTrue(accepted)
        for result in accepted:
            self.assertLess(result["child"].difference(original["rooms"][1]).area, 1e-8)
            self.assertEqual(result["rooms"][1].geom_type, "Polygon")

    def test_window_crossing_and_clear_sanitary_window_are_rejected(self):
        for y, reason in ((2.4, "window"), (1.2, "privacy")):
            original = fixture()
            original["windows"] = [
                {
                    "id": "w",
                    "rooms": [1],
                    "centre": [-0.24, y],
                    "tangent": [0.0, -1.0],
                    "polygon": rectangle([-0.24, y], [0.0, -1.0], 1.2, 0.1, 0.5),
                }
            ]
            with self.assertRaisesRegex(ValueError, reason):
                split(
                    original,
                    1,
                    shapely.box(0, 0, 2.7, 2.4),
                    shapely.LineString([(0.0, -0.06), (2.7, -0.06)]),
                    "sanitary",
                )

    def test_rigid_transform_keeps_valid_independent_support_rooms(self):
        original = fixture()
        angle = 31.0
        rotation = np.array(
            [
                [np.cos(np.deg2rad(angle)), -np.sin(np.deg2rad(angle))],
                [np.sin(np.deg2rad(angle)), np.cos(np.deg2rad(angle))],
            ]
        )
        offset = np.array([112.0, -418.0])
        transformed = original | {
            "rooms": [
                translate(rotate(p, angle, origin=(0, 0)), *offset)
                for p in original["rooms"]
            ],
            "doors": [
                d
                | {
                    "centre": (rotation @ d["centre"] + offset).tolist(),
                    "tangent": (rotation @ d["tangent"]).tolist(),
                }
                for d in original["doors"]
            ],
        }
        result, _ = candidates(transformed, 1, "sanitary", "wall-ends")
        self.assertTrue(result)
        for row in result:
            self.assertLess(row["child"].difference(transformed["rooms"][1]).area, 1e-7)
        replay, _ = candidates(transformed, 1, "sanitary", "wall-ends")
        self.assertEqual(
            [r["child"].wkb_hex for r in result], [r["child"].wkb_hex for r in replay]
        )

    def test_capacity_and_unknown_methods_cannot_expand_silently(self):
        current = fixture()
        current["rooms"] += [shapely.box(20 + i, 0, 20.5 + i, 0.5) for i in range(22)]
        current["room_kinds"] += ["generic-room"] * 22
        yes, no = candidates(current, 1, "sanitary", "wall-ends")
        self.assertFalse(yes)
        self.assertIn("capacity", no[0]["reason"])
        with self.assertRaises(ValueError):
            candidates(fixture(), 1, "sanitary", "unbounded")


if __name__ == "__main__":
    unittest.main()
