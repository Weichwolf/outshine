"""Whole functional source bodies, deferred profile work and retained valid roofs."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from broken_hip_crown_checks import check
from broken_hip_crown_plan import with_broken_hip
from demand import View
from hip_crown_checks import expected_section
from hip_crown_geometry import build as hip_build
from hip_crown_plan import with_hip
from outshine_ref.buildings.modelling import SkeletonLimits
from roof_access_plan import with_access
from structural_shell_geometry import native
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


class BrokenHipCrownTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = [plan(row, kind) for row, kind in cases()]
        cls.plans = [with_broken_hip(p) for p in cls.original]
        cls.owners = [RoofedProducts(p) for p in cls.plans]

    def test_actual_roofs_have_independent_sections_volumes_source_maps_and_holes(self):
        for owner, holes in zip(self.owners, (2, 0, 1), strict=True):
            proof = check(owner)
            self.assertEqual(proof["courtyards"], holes)
            self.assertLess(proof["volume_error_m3"], 1e-7)
            self.assertLess(proof["exact_knot_area_error_m2"], 1e-7)
            self.assertLess(proof["maximum_source_height_correction_m"], 1e-9)
            self.assertEqual(owner.roof_crown.kind, "broken-hip")
            self.assertIsNone(owner.roof_failure)

    def test_whole_functional_body_floors_frames_stairs_and_source_are_exact(self):
        for original, owner in zip(self.original, self.owners, strict=True):
            self.assertIs(owner.plan.body, original.body)
            self.assertEqual(owner.plan.source_top_m, original.source_top_m)
            before = RoofedProducts(original)
            old = before.body.facade.physical()
            new = owner.body.facade.physical()
            for name in ("shell", "stairs", "landing"):
                a, b = getattr(old.floors, name), getattr(new.floors, name)
                self.assertEqual(a.mesh.keys(), b.mesh.keys())
                for key in a.mesh:
                    np.testing.assert_array_equal(a.mesh[key], b.mesh[key])
                np.testing.assert_array_equal(a.vertex_roots, b.vertex_roots)
            for key in old.frames:
                np.testing.assert_array_equal(old.frames[key], new.frames[key])
            for key, value in before.body.draw(True).items():
                np.testing.assert_array_equal(owner.body.draw(True)[key], value)

    def test_compact_and_repeated_far_views_do_not_expand_profiles_or_skeletons(self):
        with patch(
            "broken_hip_crown_geometry.hip_build",
            side_effect=AssertionError("premature hip"),
        ):
            for request in self.plans:
                self.assertIs(with_broken_hip(request), request)
                owner = RoofedProducts(request)
                target = np.r_[
                    shapely.from_wkb(request.body.source_wkb).centroid.coords[0], 7.0
                ]
                view = View(
                    target + [0, -100000, 5000], target, width=1920, height=1080
                )
                self.assertEqual(owner.select(view)["mode"], "compact")
                self.assertIs(owner.draw(False), owner.draw(False))
                self.assertIsNone(owner.roof_components)
                self.assertIsNone(owner.body.facade.near)

    def test_every_profile_vertex_keeps_the_original_compact_displacement_bound(self):
        for owner in self.owners:
            (product,) = owner.physical_roof()
            core = owner.plan.body.core
            points = np.array(product.mesh["vertices"], copy=True)
            points[:, :2] = (points[:, :2] - core.origin_xy) @ np.array(
                [core.across_xy, core.along_xy]
            ).T
            distance = shapely.distance(
                shapely.points(points[:, :2]),
                shapely.from_wkb(owner.plan.crown.domain_wkb),
            )
            self.assertLess(distance.max(), 1e-9)
            radius = owner.plan.source_top_m - owner.plan.body.top_m
            self.assertLessEqual(
                abs(owner.plan.source_top_m - points[:, 2]).max(), radius + 1e-9
            )

    def test_capacity_refusal_keeps_exact_valid_hip_and_whole_cached_far_product(self):
        request = with_broken_hip(self.original[0], maximum_triangles=1)
        owner = RoofedProducts(request)
        owner.draw(False)
        with patch(
            "broken_hip_crown_geometry.surface", side_effect=AssertionError("early cut")
        ):
            (retained,) = owner.physical_roof()
        old = RoofedProducts(with_hip(self.original[0]))
        (expected,) = old.physical_roof()
        self.assertEqual(owner.roof_crown.kind, "hip")
        self.assertIn("capacity", owner.roof_failure)
        for key in expected.mesh:
            np.testing.assert_array_equal(retained.mesh[key], expected.mesh[key])
        np.testing.assert_array_equal(retained.vertex_roots, expected.vertex_roots)
        for key, value in old.draw(False).items():
            np.testing.assert_array_equal(owner.draw(False)[key], value)

    def test_profile_error_keeps_valid_hip_with_original_owner_and_reason(self):
        owner = RoofedProducts(self.plans[-1])
        with patch(
            "broken_hip_crown_geometry.surface", side_effect=ValueError("local profile")
        ):
            (retained,) = owner.physical_roof()
        self.assertEqual(owner.roof_crown.kind, "hip")
        self.assertEqual(owner.roof_failure, "local profile")
        self.assertEqual(retained.source_ids, (owner.roof_crown.identifier,))

    def test_base_skeleton_failure_retains_original_flat_roof_and_cached_far(self):
        original = self.original[1]
        owner = RoofedProducts(
            with_broken_hip(original, limits=SkeletonLimits(nodes=1))
        )
        owner.draw(False)
        products = owner.physical_roof()
        self.assertIs(owner.roof_crown, original.crown)
        self.assertTrue(owner.roof_failure)
        expected = RoofedProducts(original)
        for a, b in zip(products, expected.physical_roof(), strict=True):
            for key in a.mesh:
                np.testing.assert_array_equal(a.mesh[key], b.mesh[key])
        for key, value in expected.draw(False).items():
            np.testing.assert_array_equal(owner.draw(False)[key], value)

    def test_existing_highest_roof_access_is_not_replaced(self):
        accessible = with_access(self.original[-1])
        self.assertIsNotNone(accessible.access)
        self.assertIs(with_broken_hip(accessible), accessible)
        self.assertIsNone(with_access(self.plans[-1]).access)

    def test_invalid_requests_and_changed_source_height_cannot_rebind(self):
        for options in (
            {"run_fraction": 0},
            {"height_fraction": 1},
            {"run_fraction": float("nan")},
            {"profile_method": "unknown"},
            {"maximum_triangles": 65537},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                with_broken_hip(self.original[0], **options)
        with self.assertRaisesRegex(ValueError, "unchanged bound"):
            replace(self.plans[0].crown, top_m=self.plans[0].source_top_m + 1)

    def test_existing_facet_union_cannot_return_a_wrong_original_roof_reference(self):
        crown = self.plans[1].crown
        (original,), _ = hip_build(crown.base_hip)
        inverse = 0.15 + (0.6501 - 0.65) * 0.85 / 0.35
        rise = crown.top_m - crown.deck_m
        expected = native(original.mesh, original.vertex_roots).scale((1000,) * 3)
        area = expected.slice((crown.deck_m + rise * inverse) * 1000).area() / 1e6
        actual = expected_section(
            original, rise * inverse / original.hip_plan.factor(rise, "byHeight")
        )
        self.assertTrue(actual.is_valid)
        self.assertLess(abs(actual.area - area), 1e-7)

    def test_logical_composition_matches_complete_closed_profile_contract(self):
        for original in self.original:
            owner = RoofedProducts(with_broken_hip(original, profile_method="logical"))
            proof = check(owner)
            self.assertLess(proof["area_error_m2"], 1e-7)
            (product,) = owner.physical_roof()
            self.assertEqual(product.profile_work["logical_split_outputs"], 2)
            with self.assertRaises(ValueError):
                product.source_triangle_ids.setflags(write=True)

    def test_nondefault_profile_keeps_the_same_bound_source_and_exact_knot_contract(
        self,
    ):
        owner = RoofedProducts(
            with_broken_hip(self.plans[1], run_fraction=0.25, height_fraction=0.75)
        )
        proof = check(owner)
        self.assertLess(proof["exact_knot_area_error_m2"], 1e-7)
        self.assertEqual(owner.roof_crown.run_fraction, 0.25)
        self.assertIs(owner.plan.body, self.original[1].body)
        self.assertIs(owner.roof_crown.base_hip, self.plans[1].crown.base_hip)


if __name__ == "__main__":
    unittest.main()
