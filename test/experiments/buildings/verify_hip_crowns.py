"""Source-height hips, retained bodies, deferred work and complete local fallback."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from hip_crown_checks import check
from hip_crown_geometry import enu
from hip_crown_plan import with_hip
from outshine_ref.buildings.modelling import SkeletonLimits
from roof_access_plan import with_access
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


class HipCrownTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = cases()
        cls.original = [plan(row, kind) for row, kind in cls.sources]
        cls.owners = [RoofedProducts(with_hip(p)) for p in cls.original]

    def test_actual_closed_roofs_keep_height_eaves_and_all_courtyards(self):
        for owner, holes in zip(self.owners, (2, 0, 1), strict=True):
            result = check(owner)
            self.assertEqual(result["effective_kind"], "hip")
            self.assertEqual(result["courtyards"], holes)
            self.assertLess(result["volume_error_m3"], 1e-7)
            self.assertTrue(all(e < 1e-7 for e in result["section_xor_m2"]))

    def test_functional_body_buffers_and_source_identity_are_exact(self):
        for old, owner in zip(self.original, self.owners, strict=True):
            self.assertIs(owner.plan.body, old.body)
            self.assertEqual(owner.plan.source_top_m, old.source_top_m)
            before = RoofedProducts(old).body.facade.physical()
            after = owner.body.facade.physical()
            for name in ("shell", "stairs", "landing"):
                a, b = getattr(before.floors, name), getattr(after.floors, name)
                self.assertEqual(a.mesh.keys(), b.mesh.keys())
                for key in a.mesh:
                    np.testing.assert_array_equal(a.mesh[key], b.mesh[key])
                np.testing.assert_array_equal(a.vertex_roots, b.vertex_roots)
            for key in before.frames:
                np.testing.assert_array_equal(before.frames[key], after.frames[key])

    def test_far_and_repeated_views_never_invoke_a_skeleton(self):
        with patch(
            "hip_crown_geometry.prepare_hip",
            side_effect=AssertionError("premature skeleton"),
        ):
            for p in self.original:
                current = with_hip(p)
                self.assertIs(with_hip(current), current)
                owner = RoofedProducts(current)
                target = np.r_[
                    shapely.from_wkb(p.body.source_wkb).centroid.coords[0], 7.0
                ]
                view = View(
                    target + [0, -100000, 5000], target, width=1920, height=1080
                )
                self.assertEqual(owner.select(view)["mode"], "compact")
                self.assertIs(owner.draw(False), owner.draw(False))
                self.assertIsNone(owner.roof_components)
                self.assertIsNone(owner.body.facade.near)

    def test_node_budget_restores_old_near_and_previously_cached_far_products(self):
        p = self.original[1]
        owner = RoofedProducts(with_hip(p, limits=SkeletonLimits(nodes=1)))
        candidate = owner.draw(False)
        roof = owner.physical_roof()
        self.assertTrue(owner.roof_failure)
        self.assertIs(owner.roof_crown, p.crown)
        self.assertIs(roof, owner.physical_roof())
        self.assertIsNone(owner.body.facade.near)
        before = RoofedProducts(p)
        for old, actual in zip(before.physical_roof(), roof, strict=True):
            for key in old.mesh:
                np.testing.assert_array_equal(old.mesh[key], actual.mesh[key])
        restored = owner.draw(False)
        self.assertIsNot(candidate, restored)
        for near in (False, True):
            for key, value in before.draw(near).items():
                np.testing.assert_array_equal(value, owner.draw(near)[key])

    def test_contour_keeps_every_source_even_when_a_local_roof_fails(self):
        for original in self.original:
            owner = RoofedProducts(with_hip(original, method="contour"))
            result = check(owner)
            self.assertIs(owner.plan.body, original.body)
            self.assertIn(result["effective_kind"], ("hip", original.crown.kind))
            if result["effective_kind"] != "hip":
                self.assertTrue(result["failure"])
                self.assertIs(owner.roof_crown, original.crown)

    def test_source_vertex_budget_and_existing_roof_access_are_preserved(self):
        current = with_hip(self.original[0], limits=SkeletonLimits(vertices=3))
        self.assertIs(current.crown, self.original[0].crown)
        self.assertIn("vertex budget", current.reason)
        accessible = with_access(self.original[-1])
        self.assertIsNotNone(accessible.access)
        self.assertIs(with_hip(accessible), accessible)
        hip = with_hip(self.original[-1])
        self.assertIsNone(with_access(hip).access)

    def test_all_fine_vertices_have_the_declared_vertical_coarse_position_bound(self):
        for owner in self.owners:
            product = owner.physical_roof()[0]
            core = owner.plan.body.core
            vertices = np.array(product.mesh["vertices"], copy=True)
            vertices[:, :2] = (vertices[:, :2] - core.origin_xy) @ np.array(
                [core.across_xy, core.along_xy]
            ).T
            crown = owner.plan.crown
            distances = shapely.distance(
                shapely.points(vertices[:, :2]), shapely.from_wkb(crown.domain_wkb)
            )
            self.assertLess(float(distances.max()), 1e-9)
            self.assertLessEqual(
                float((crown.top_m - vertices[:, 2]).max()),
                crown.top_m - crown.deck_m + 1e-9,
            )

    def test_cga_to_enu_conversion_preserves_handedness_and_declared_up(self):
        self.assertEqual(float(np.linalg.det(enu(np.eye(3)))), 1.0)
        np.testing.assert_array_equal(
            enu(np.array([[0, 1, 0], [0, 0, -1]])), [[0, 0, 1], [0, 1, 0]]
        )


if __name__ == "__main__":
    unittest.main()
