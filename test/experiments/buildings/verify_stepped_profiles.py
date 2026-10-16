"""Independent profile cuts and actual source geometry/access controls."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from building_vertical import build
from shapely.affinity import rotate, translate
from stepped_envelope_geometry import geometry
from stepped_form_plan import choose
from stepped_form_profiles import profile_for, requests
from stepped_profile_probe import sources
from stepped_source import exterior, near, plan_for
from verify_vertical import connected_spaces


class ProfileContours(unittest.TestCase):
    def test_corner_cuts_equal_independent_metric_rectangles(self):
        polygon = shapely.box(0, 0, 24, 18)
        axes = np.eye(2)
        cases = list(requests({"footprint": polygon}, axes, [0, 1.2, 2.4], "corner"))
        self.assertEqual(len(cases), 4)
        for case, (a, b) in zip(
            cases, ((-1, -1), (-1, 1), (1, -1), (1, 1)), strict=True
        ):
            for actual, distance in zip(case[3], (0, 1.2, 2.4), strict=True):
                expected = shapely.box(
                    distance if a < 0 else 0,
                    distance if b < 0 else 0,
                    24 - distance if a > 0 else 24,
                    18 - distance if b > 0 else 18,
                )
                self.assertLess(actual.symmetric_difference(expected).area, 1e-10)

    def test_diagonal_cut_equals_analytic_corner_triangle(self):
        polygon = shapely.box(0, 0, 24, 18)
        cases = list(requests({"footprint": polygon}, np.eye(2), [0, 2.8], "diagonal"))
        actual = cases[-1][3][-1]
        side = 2.8 * np.sqrt(2)
        removed = shapely.Polygon([(24, 18), (24 - side, 18), (24, 18 - side)])
        self.assertLess(
            actual.symmetric_difference(polygon.difference(removed)).area, 1e-10
        )
        self.assertAlmostEqual(polygon.area - actual.area, 2.8**2)

    def test_rotated_courtyard_cuts_keep_holes_source_and_nesting(self):
        polygon = shapely.Polygon(
            [(0, 0), (24, 0), (24, 18), (0, 18)],
            [[(9, 6), (9, 12), (15, 12), (15, 6)]],
        )
        angle = np.radians(31)
        matrix = np.array(
            [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
        )
        moved = translate(rotate(polygon, 31, origin=(0, 0)), 1300, -2100)
        for profile in ("corner", "diagonal"):
            original = list(
                requests({"footprint": polygon}, np.eye(2), [0, 1.2, 2.4], profile)
            )
            transformed = list(
                requests({"footprint": moved}, matrix, [0, 1.2, 2.4], profile)
            )
            for a, b in zip(original, transformed, strict=True):
                previous = moved
                for source, actual in zip(a[3], b[3], strict=True):
                    expected = translate(rotate(source, 31, origin=(0, 0)), 1300, -2100)
                    self.assertLess(expected.symmetric_difference(actual).area, 1e-9)
                    self.assertLess(actual.difference(previous).area, 1e-9)
                    self.assertGreaterEqual(len(actual.interiors), 1)
                    previous = actual

    def test_unknown_profiles_reject_before_search_and_identity_has_a_separate_stream(
        self,
    ):
        with (
            patch("stepped_form_plan.frame", side_effect=AssertionError("expanded")),
            self.assertRaisesRegex(ValueError, "Unknown"),
        ):
            choose({}, profile="unknown")
        ids = [row["id"] for row in sources()]
        forward = {key: profile_for(key) for key in ids}
        self.assertEqual(forward, {key: profile_for(key) for key in reversed(ids)})
        self.assertEqual(set(forward.values()), {"corner", "diagonal"})


class SourceProfiles(unittest.TestCase):
    def test_actual_source_profiles_keep_room_core_ids_and_real_mesh_access(self):
        for index, profile in ((0, "corner"), (2, "diagonal")):
            row = sources()[index]
            original = row["polygon"].wkb
            plan = plan_for(row, profile)
            base = build(
                {
                    "id": row["id"],
                    "polygon": row["polygon"],
                    "properties": row["properties"],
                },
                body_top_m=plan["body_top_m"],
            )
            self.assertTrue(plan["form_recipe"]["method"].startswith(profile))
            self.assertEqual(row["polygon"].wkb, original)
            self.assertEqual(plan["storey_room_ids"], base["storey_room_ids"])
            self.assertTrue(plan["storey_plans"][0]["footprint"].equals(row["polygon"]))
            self.assertTrue(plan["core_envelope"].equals(base["core_envelope"]))
            previous = row["polygon"]
            for floor in plan["storey_plans"]:
                self.assertLess(floor["footprint"].difference(previous).area, 1e-7)
                self.assertGreaterEqual(min(room.area for room in floor["rooms"]), 5)
                previous = floor["footprint"]
            connected_spaces(plan)
            _, access, terraces, _ = near(plan, 1)
            self.assertGreater(access["sampled_poses"], 100)
            self.assertTrue(terraces)
            geometry(plan, exterior(plan, 1)[2])


if __name__ == "__main__":
    unittest.main()
