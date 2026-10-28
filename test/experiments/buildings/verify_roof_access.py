"""Source-height roof access, retained lower stairs and actual doorway clearance."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from building_stair import core_layout
from demand import View
from roof_access_checks import check
from roof_access_plan import with_access
from terraced_crown_geometry import ROOF_MATERIAL, RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_mass_frame import local_plan


class RoofAccessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = cases()
        cls.original = [plan(row, kind) for row, kind in cls.sources]
        cls.plans = [with_access(p) for p in cls.original]
        cls.owner = RoofedProducts(cls.plans[-1])

    def test_real_low_reserves_keep_complete_original_body_and_crown(self):
        for old, current in zip(self.original[:2], self.plans[:2], strict=True):
            self.assertIsNone(current.access)
            self.assertIs(current.body, old.body)
            self.assertIs(current.crown, old.crown)
            self.assertIn("unserved", current.reason)
        low_guard = replace(self.original[-1].crown, guard_m=0.9)
        self.assertIsNone(
            with_access(replace(self.original[-1], crown=low_guard)).access
        )

    def test_access_preserves_source_and_storeys_and_is_idempotent(self):
        old, current = self.original[-1], self.plans[-1]
        self.assertIs(current.body, old.body)
        self.assertEqual(current.source_top_m, old.source_top_m)
        self.assertEqual(current.crown.deck_m, old.crown.deck_m)
        self.assertIs(with_access(current), current)
        self.assertGreater(current.access.clear_height_m, 2.10)

    def test_zero_and_insufficient_header_reserve_reject_before_stair_work(self):
        old = self.original[-1]
        with patch(
            "roof_access_plan.core_layout",
            side_effect=AssertionError("unneeded stair work"),
        ):
            for clearance in (2.10, 2.12, 2.14):
                current = with_access(
                    replace(old, source_top_m=old.crown.deck_m + clearance + 0.08)
                )
                self.assertIsNone(current.access)
                self.assertIs(current.body, old.body)
                self.assertIs(current.crown, old.crown)

    def test_last_flight_preserves_original_opening_and_tread_count(self):
        current = self.plans[-1]
        body, access = local_plan(current.body), current.access
        core = shapely.from_wkb(body.core.footprint_wkb)
        final = core_layout(
            core, access.rise_m, access.bottom_m, risers=body.core.risers
        )
        automatic = core_layout(core, access.rise_m, access.bottom_m)
        self.assertEqual(final["opening"].wkb, body.core.opening_wkb)
        self.assertEqual(final["opening"].wkb, access.opening_wkb)
        self.assertLess(automatic["risers"], final["risers"])
        self.assertNotEqual(automatic["opening"].wkb, final["opening"].wkb)
        self.assertAlmostEqual(final["path"][-1, 2], current.crown.deck_m)
        for invalid in (True, 0, 13, 15.0):
            with self.assertRaises(ValueError):
                core_layout(core, access.rise_m, access.bottom_m, risers=invalid)

    def test_actual_closed_sections_stairs_and_continuous_doorway(self):
        proof = check(self.owner)
        self.assertEqual(proof["roof_stair_poses"], 18)
        self.assertGreater(proof["sampled_clearance_m"], 1.90)
        self.assertGreater(proof["existing_body"]["sampled_stair_poses"], 100)
        self.assertGreater(proof["continuous_roof_door_route"]["length_m"], 1.0)

    def test_visible_roof_deck_has_the_same_aperture_as_physical_slabs(self):
        owner = self.owner
        mesh = owner.draw(True)
        xyz = mesh["vertices"][mesh["faces"]]
        cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        level = np.max(abs(xyz[:, :, 2] - owner.plan.crown.deck_m), axis=1) < 1e-9
        keep = level & (cross[:, 2] > 0) & (mesh["materials"] == ROOF_MATERIAL)
        projected = shapely.union_all([shapely.Polygon(t[:, :2]) for t in xyz[keep]])
        core = owner.plan.body.core
        from terraced_facade_prepare import positions

        local = shapely.from_wkb(owner.plan.crown.deck_wkb)
        from shapely.affinity import affine_transform

        expected = affine_transform(
            local,
            [*np.array([core.across_xy, core.along_xy]).T.ravel(), *core.origin_xy],
        )
        self.assertLess(projected.symmetric_difference(expected).area, 1e-7)
        self.assertIs(mesh, owner.draw(True))
        self.assertTrue(
            np.isfinite(positions(np.asarray(owner.plan.access.route_xyz), core)).all()
        )

    def test_far_and_lookaway_keep_near_builds_unexpanded(self):
        current = self.plans[-1]
        target = np.r_[self.sources[-1][0]["polygon"].centroid.coords[0], 12.0]
        view = View(target + [0, -100000, 5000], target, width=1920, height=1080)
        with (
            patch(
                "terraced_facade_geometry.build",
                side_effect=AssertionError("near body"),
            ),
            patch(
                "terraced_crown_geometry.union_prisms",
                side_effect=AssertionError("near roof"),
            ),
        ):
            owner = RoofedProducts(current)
            self.assertEqual(owner.select(view)["mode"], "compact")
            owner.draw(False)
            self.assertFalse(
                owner.select(replace(view, target=view.eye + [0, -1, 0]))[
                    "possibly_visible"
                ]
            )
            self.assertIsNone(owner.body.facade.near)
            self.assertIsNone(owner.roof_components)


if __name__ == "__main__":
    unittest.main()
