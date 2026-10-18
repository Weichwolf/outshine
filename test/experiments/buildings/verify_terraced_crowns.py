"""Coupled actual roofs: finite-actor body, height/holes and direct-before-near demand."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from terraced_crown_geometry import ROOF_MATERIAL, RoofedProducts, capless
from terraced_crown_plan import plan
from terraced_crown_probe import cases, roof_checks
from terraced_mass_admission import inspect
from terraced_mass_plan import plan as mass_plan


class CoupledRoofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = cases()
        cls.owners = [RoofedProducts(plan(r, kind)) for r, kind in cls.cases]

    def test_actual_body_stairs_arrivals_and_terraces_still_pass(self):
        for owner in self.owners:
            with self.subTest(source=owner.plan.body.identifier):
                result = inspect(owner.body)
                self.assertGreater(result["sampled_stair_poses"], 0)

    def test_closed_roofs_keep_original_height_contours_and_courtyards(self):
        for (row, kind), owner in zip(self.cases, self.owners, strict=True):
            with self.subTest(source=row["id"]):
                old, new = mass_plan(row, kind), owner.plan.body
                self.assertEqual(new.source_wkb, old.source_wkb)
                self.assertEqual(len(new.storeys), len(old.storeys))
                self.assertEqual(owner.plan.source_top_m, old.top_m)
                self.assertEqual(new.top_m, owner.plan.crown.deck_m)
                self.assertGreaterEqual(
                    min(s.top_m - s.bottom_m for s in new.storeys), 2.64
                )
                self.assertLessEqual(owner.plan.attempts, 2)
                self.assertLessEqual(roof_checks(owner)["footprint_xor_m2"], 1e-7)

    def test_composed_draw_removes_only_the_body_contact_cap(self):
        for owner in self.owners:
            mesh = owner.draw(True)
            xyz = mesh["vertices"][mesh["faces"]]
            cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
            flat = np.max(abs(xyz[:, :, 2] - owner.plan.body.top_m), axis=1) < 1e-9
            flat &= cross[:, 2] > 0
            self.assertTrue(np.all(mesh["materials"][flat] == ROOF_MATERIAL))
            deck = shapely.from_wkb(owner.plan.crown.deck_wkb)
            self.assertAlmostEqual(
                float(cross[flat, 2].sum() / 2),
                deck.area,
                delta=1e-7,
            )
            self.assertEqual(mesh["faces"].dtype, np.uint32)
            self.assertIs(mesh, owner.draw(True))

    def test_far_lookaway_and_return_reuse_one_plan_without_near_expansion(self):
        row, kind = self.cases[0]
        target = np.r_[row["polygon"].centroid.coords[0], 6.0]
        far = View(target + [0, -100000, 5000], target, width=1920, height=1080)
        with (
            patch(
                "terraced_crown_geometry.union_prisms",
                side_effect=AssertionError("near roof"),
            ),
            patch(
                "terraced_facade_geometry.build",
                side_effect=AssertionError("near body"),
            ),
        ):
            owner = RoofedProducts(plan(row, kind))
            self.assertEqual(owner.select(far)["mode"], "compact")
            mesh = owner.draw(False)
            self.assertIs(mesh, owner.draw(False))
            away = replace(far, target=far.eye + [0, -1, 0])
            self.assertFalse(owner.select(away)["possibly_visible"])
            self.assertIsNone(owner.roof_components)
            self.assertIsNone(owner.body.facade.near)
        inside = replace(far, eye=target, target=target + [1, 0, 0])
        self.assertEqual(owner.select(inside)["mode"], "near")
        a = owner.physical_roof()
        self.assertIs(a, owner.physical_roof())

    def test_supplied_levels_roofs_and_unsafe_reserves_are_preserved_or_rejected(self):
        row, kind = self.cases[0]
        for key in ("building:levels", "building_levels", "levels"):
            candidate = dict(row, properties=dict(row["properties"], **{key: 5}))
            result = plan(candidate, kind)
            self.assertTrue(result.body.source_levels)
            self.assertEqual(len(result.body.storeys), 5)
        with self.assertRaisesRegex(ValueError, "Supplied roof"):
            plan(
                dict(
                    row, properties=dict(row["properties"], **{"roof:shape": "gable"})
                ),
                kind,
            )
        for value in (-1, 100, float("nan"), True):
            with self.assertRaises(ValueError):
                mass_plan(row, kind, roof_height_m=value)

    def test_visible_roof_detail_does_not_expand_the_distant_building_body(self):
        row, kind = self.cases[0]
        target = np.r_[row["polygon"].centroid.coords[0], 6.0]
        middle = View(target + [0, -3000, 500], target, width=1920, height=1080)
        with patch(
            "terraced_facade_geometry.build",
            side_effect=AssertionError("unneeded full body"),
        ):
            owner = RoofedProducts(plan(row, kind))
            parts, decision = owner.resolve(middle)
            self.assertEqual(decision["body_mode"], "compact")
            self.assertEqual(decision["roof_mode"], "near")
            self.assertEqual(decision["mode"], "mixed")
            self.assertIsNone(owner.body.facade.near)
            self.assertIsNotNone(owner.roof_components)
            again, _decision = owner.resolve(replace(middle, movement_m=1.0))
            for first, second in zip(parts, again, strict=True):
                self.assertIs(first, second)

    def test_a_missing_contact_cap_is_not_silently_accepted(self):
        owner = self.owners[0]
        first = capless(owner.body.draw(False), owner.plan.body.top_m)
        with self.assertRaisesRegex(ValueError, "highest body cap"):
            capless(first, owner.plan.body.top_m)

    def test_missing_service_placement_replans_the_body_at_the_actual_parapet(self):
        row, _kind = self.cases[0]
        result = plan(row, "paired-wings")
        self.assertEqual(result.attempts, 2)
        self.assertEqual(result.crown.kind, "parapet")
        self.assertEqual(result.crown.deck_m, result.body.top_m)
        roof_checks(RoofedProducts(result))

    def test_no_roof_height_budget_retains_a_complete_ordinary_body(self):
        row, kind = self.cases[0]
        row = dict(row, properties={"render_height": 7.92})
        result = plan(row, kind)
        self.assertIsNone(result.crown)
        self.assertEqual(result.body.top_m, result.source_top_m)
        owner = RoofedProducts(result)
        self.assertEqual(len(owner.parts(False, False)), 1)
        self.assertIsNone(owner.physical_roof())


if __name__ == "__main__":
    unittest.main()
