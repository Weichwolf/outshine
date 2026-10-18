"""Independent direct-surface measures and actual demand-before-expansion controls."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from terraced_facade_geometry import build_facade
from terraced_facade_prepare import prepare
from terraced_facade_products import FacadeProducts
from terraced_facade_surface import compact
from terraced_mass_plan import plan
from terraced_mass_sources import sources


class FacadeProductTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = [
            prepare(plan(row, kind))
            for row in sources()
            for kind in ("cascade", "paired-wings")
        ]
        cls.surfaces = [compact(p) for p in cls.plans]

    def test_direct_surface_never_builds_near_floors_booleans_or_frame_templates(self):
        with (
            patch(
                "terraced_facade_geometry.build",
                side_effect=AssertionError("floor expansion"),
            ),
            patch(
                "terraced_facade_geometry.details",
                side_effect=AssertionError("detail expansion"),
            ),
            patch(
                "terraced_facade_geometry.subtract_prisms",
                side_effect=AssertionError("3D Boolean"),
            ),
        ):
            for p, expected in zip(self.plans, self.surfaces, strict=True):
                actual = compact(p)
                for key in actual.mesh:
                    np.testing.assert_array_equal(actual.mesh[key], expected.mesh[key])

    def test_original_profile_roofs_and_underside_retain_independent_area_and_height(
        self,
    ):
        for p, surface in zip(self.plans, self.surfaces, strict=True):
            xyz = surface.mesh["vertices"][surface.mesh["faces"]]
            cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
            area = np.linalg.norm(cross, axis=1) / 2
            horizontal = abs(cross[:, 2]) > np.linalg.norm(cross, axis=1) * 0.99
            body = surface.mesh["materials"] == 12
            source_area = shapely.from_wkb(p.mass.source_wkb).area
            self.assertAlmostEqual(
                float(area[horizontal & body & (cross[:, 2] > 0)].sum()),
                source_area,
                delta=1e-7,
            )
            self.assertAlmostEqual(
                float(area[horizontal & body & (cross[:, 2] < 0)].sum()),
                source_area,
                delta=1e-7,
            )
            self.assertAlmostEqual(float(xyz[:, :, 2].min()), p.mass.bottom_m)
            self.assertAlmostEqual(float(xyz[:, :, 2].max()), p.mass.top_m)

    def test_direct_wall_and_glazing_areas_follow_the_logical_profiles_and_openings(
        self,
    ):
        for p, surface in zip(self.plans, self.surfaces, strict=True):
            xyz = surface.mesh["vertices"][surface.mesh["faces"]]
            cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
            area = np.linalg.norm(cross, axis=1) / 2
            vertical = abs(cross[:, 2]) <= np.linalg.norm(cross, axis=1) * 1e-8
            glazing = sum(w.width_m * w.height_m for w in p.facade.windows)
            self.assertAlmostEqual(
                float(area[surface.mesh["materials"] == 3].sum()), glazing, delta=1e-7
            )
            expected = (
                sum(
                    shapely.from_wkb(s.footprint_wkb).length * (s.top_m - s.bottom_m)
                    for s in p.mass.storeys
                )
                - 1.10 * p.mass.entry.height_m
            )
            body = np.isin(surface.mesh["materials"], (3, 12))
            self.assertAlmostEqual(
                float(area[vertical & body].sum()), expected, delta=1e-7
            )

    def test_window_identity_provenance_normals_and_readonly_buffers(self):
        for p, s in zip(self.plans, self.surfaces, strict=True):
            self.assertEqual(
                s.window_ids, tuple(w.identifier for w in p.facade.windows)
            )
            self.assertEqual(len(s.triangle_owners), len(s.mesh["faces"]))
            self.assertEqual(s.mesh["faces"].dtype, np.dtype(np.uint32))
            self.assertLess(int(s.triangle_owners.max()), len(s.owner_ids))
            self.assertTrue(set(s.window_ids).issubset(set(s.owner_ids)))
            for array in s.mesh.values():
                self.assertTrue(np.isfinite(array).all())
                self.assertFalse(array.flags.writeable)
            np.testing.assert_allclose(
                np.linalg.norm(s.mesh["normals"], axis=1), 1, rtol=0, atol=1e-12
            )

    def test_capacity_rejection_precedes_closed_floor_construction(self):
        row = {
            "id": "oversize-window-control",
            "polygon": shapely.box(0, 0, 100, 80),
            "properties": {"render_height": 54, "building:levels": 18},
        }
        mass = plan(row, "cascade")
        with (
            patch(
                "terraced_facade_geometry.build",
                side_effect=AssertionError("unneeded floor build"),
            ),
            self.assertRaisesRegex(ValueError, "exceeds768"),
        ):
            build_facade(mass)

    def test_wrong_blueprint_rejected_before_expansion(self):
        with (
            patch(
                "terraced_facade_geometry.build",
                side_effect=AssertionError("unneeded floor build"),
            ),
            self.assertRaisesRegex(ValueError, "different source mass"),
        ):
            build_facade(self.plans[0].mass, prepared=self.plans[1])

    def test_rotation_movement_and_interaction_reuse_one_owner_without_replanning(self):
        p = self.plans[0]
        centre = np.r_[
            shapely.from_wkb(p.mass.source_wkb).centroid.coords[0], p.mass.top_m / 2
        ]
        far = View(centre + [0, -1400, 700], centre, width=1920, height=1080)
        away = replace(far, target=far.eye + (far.eye - far.target))
        moving = replace(far, movement_m=1800)
        point = shapely.from_wkb(p.mass.source_wkb).representative_point()
        interior = np.r_[point.coords[0], 1.7]
        inside = View(interior, interior + [1, 0, 0], width=1920, height=1080)
        products = FacadeProducts(p)
        marker = object()
        with (
            patch(
                "terraced_facade_products.shapely.from_wkb",
                side_effect=AssertionError("repeated source decode"),
            ),
            patch(
                "terraced_facade_products.build_facade", return_value=marker
            ) as build,
        ):
            for view in (far, away, far):
                result, decision = products.resolve(view)
                self.assertIs(result, products.coarse)
                self.assertEqual(decision["mode"], "compact")
            build.assert_not_called()
            for view in (moving, inside):
                result, decision = products.resolve(view)
                self.assertIs(result, marker)
                self.assertEqual(decision["mode"], "near")
            self.assertEqual(build.call_count, 1)
            self.assertIs(products.physical(), marker)
            self.assertIs(products.resolve(away)[0], products.coarse)
            self.assertIs(products.physical(), marker)
            self.assertEqual(build.call_count, 1)


if __name__ == "__main__":
    unittest.main()
