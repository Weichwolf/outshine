"""Actual source selection and unchanged bindings for coupled whole-city roofs."""

import unittest
from unittest.mock import patch

import numpy as np
from demand import View
from terraced_city_parts import PreviousParts
from terraced_crown_plan import ROOF_MATERIAL
from terraced_crown_probe import cases
from terraced_mass_census import source_identity
from tile_terraced_crowns import CrownedDetails, crown_definitions
from verify_terraced_city import Previous


class CrownCityTests(unittest.TestCase):
    def setUp(self):
        self.row, programme = cases()[0]
        self.previous = Previous([self.row])
        self.previous.view = View(
            self.previous.centre + [0, -30000, 10000],
            self.previous.centre,
            width=1920,
            height=1080,
        )
        self.parts = PreviousParts(self.previous)
        self.record = source_identity(self.row) | {
            "programme": programme,
            "passed": True,
            "full_body_checked": True,
            "full_draw_checked": True,
            "roof_components_passed": True,
            "checks": {"scope": "test selection only, not a physical audit"},
        }

    def test_roof_only_or_failed_body_reference_cannot_select_a_changed_city_body(self):
        for field in (
            "passed",
            "full_body_checked",
            "full_draw_checked",
            "roof_components_passed",
        ):
            for invalid in (False, None, "true"):
                with self.assertRaisesRegex(ValueError, "roof and body"):
                    CrownedDetails(
                        self.parts, {self.row["id"]: self.record | {field: invalid}}
                    )

    def test_unselected_source_keeps_the_exact_previous_draw_without_roof_planning(
        self,
    ):
        before = self.parts(self.row, {}, 14)
        builder = CrownedDetails(self.parts, {})
        with patch(
            "tile_terraced_crowns.plan", side_effect=AssertionError("roof plan")
        ):
            after = builder(self.row, {}, 14)
        self.assertIs(after, before)
        self.assertEqual(self.previous.calls, 1)
        self.assertFalse(builder.assets)

    def test_actual_far_source_keeps_roof_body_lazy_and_materials_preserve_source_wall(
        self,
    ):
        builder = CrownedDetails(self.parts, {self.row["id"]: self.record})
        with patch(
            "terraced_facade_geometry.build",
            side_effect=AssertionError("unrequested full body"),
        ):
            mesh, info = builder(self.row, {}, 14)
        asset = builder.assets[self.row["id"]]
        self.assertIsNone(asset.body.facade.near)
        self.assertIsNone(asset.roof_components)
        self.assertEqual(info["product"], "compact")
        self.assertEqual(mesh["faces"].dtype, np.dtype(np.uint32))
        self.assertIn(14, mesh["materials"])
        self.assertIn(ROOF_MATERIAL, mesh["materials"])
        self.assertNotIn(12, mesh["materials"])
        self.assertEqual(set(crown_definitions()), {"60000", "60001", "60002", "60003"})
        self.assertEqual(mesh["vertices"][:, 2].max(), asset.plan.source_top_m)
        with self.assertRaisesRegex(ValueError, "prepared source"):
            builder(dict(self.row), {}, 14)
        self.previous.view = View(
            self.previous.centre + [0, -30000, 12000],
            self.previous.centre,
            width=1920,
            height=1080,
        )
        with patch("tile_terraced_crowns.plan", side_effect=AssertionError("new plan")):
            builder(self.row, {}, 14)
        self.assertIs(builder.assets[self.row["id"]], asset)

    def test_middle_city_view_expands_only_the_roof_and_reuses_it_after_motion(self):
        builder = CrownedDetails(self.parts, {self.row["id"]: self.record})
        self.previous.view = View(
            self.previous.centre + [0, -3000, 1000],
            self.previous.centre,
            width=1920,
            height=1080,
        )
        with patch(
            "terraced_facade_geometry.build",
            side_effect=AssertionError("roof demand forced full body"),
        ):
            _mesh, info = builder(self.row, {}, 14)
            self.assertEqual(info["product"], "mixed")
            asset = builder.assets[self.row["id"]]
            roof = asset.roof_components
            self.assertIsNotNone(roof)
            self.assertIsNone(asset.body.facade.near)
            self.previous.view = View(
                self.previous.centre + [1, -3000, 1000],
                self.previous.centre,
                width=1920,
                height=1080,
            )
            builder(self.row, {}, 14)
        self.assertIs(asset.roof_components, roof)
        self.assertIsNone(asset.body.facade.near)


if __name__ == "__main__":
    unittest.main()
