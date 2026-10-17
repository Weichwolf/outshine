"""Actual attached volumes, complete pockets, matching seams and old source owners."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from dormer_assets import prepare
from dormer_geometry import union
from dormer_plan import plan
from dormer_sources import sources
from dormer_windows import DEPTH_M, aperture
from facade_parts import append
from provider_contract import attributes
from outshine_ref.buildings.modelling.skeleton_plan import SkeletonBudgetExceeded
from structural_shell_geometry import native
from tile_dormers import DormerFacades
from tile_elevations import ElevationFacades
from tile_facade_recess import selection
from tile_roof_envelopes import choose
from verify_elevations import build, untimed
from verify_facade_frames import view_for


class DormerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources()
        cls.original = copy.deepcopy(cls.rows)
        cls.assets = [
            prepare(
                row,
                attributes(row["properties"]),
                choose(row, attributes(row["properties"])),
            )
            for row, _nearby in cls.rows
        ]

    def test_real_plans_are_disjoint_contained_below_original_top_with_useful_fronts(
        self,
    ):
        for (row, _neighbors), asset in zip(self.rows, self.assets, strict=True):
            regions = [shapely.from_wkb(p.footprint_wkb) for p in asset.dormers]
            self.assertTrue(asset.dormers)
            for p, region in zip(asset.dormers, regions, strict=True):
                self.assertTrue(row["polygon"].covers(region))
                self.assertGreaterEqual(p.cap_m - p.base_m, 2.0)
                self.assertLess(
                    p.cap_m, attributes(row["properties"])["buildingHeight"]
                )
            self.assertAlmostEqual(
                sum(p.area for p in regions),
                shapely.union_all(regions).area,
                delta=1e-8,
            )
        self.assertEqual(self.rows, self.original)

    def test_closed_union_matches_independent_affine_roof_integral_and_batch_alternative(
        self,
    ):
        for asset in self.assets:
            solid = native(asset.full.physical.mesh, asset.full.physical.vertex_roots)
            expected = asset.original.volume_m3 + sum(
                p.added_volume_m3 for p in asset.dormers
            )
            self.assertAlmostEqual(solid.volume(), expected, delta=1e-7)
            alternative = union(asset.original, asset.dormers, method="sequential")
            other = native(alternative.mesh, alternative.vertex_roots)
            self.assertLess((solid - other).volume() + (other - solid).volume(), 1e-7)
            self.assertEqual(
                set(asset.full.physical.source_ids[1:]),
                {p.identifier for p in asset.dormers},
            )
            self.assertEqual(
                set(asset.full.physical.source_indices),
                set(range(len(asset.dormers) + 1)),
            )

    def test_all_near_pockets_remove_exact_volume_and_body_skin_seams_remain_closed(
        self,
    ):
        for asset in self.assets:
            near = asset.near_products()
            expected = sum(
                aperture(p)[1] * aperture(p)[2] * DEPTH_M for p in asset.dormers
            )
            self.assertAlmostEqual(
                asset.full.physical.volume_m3 - near.physical.volume_m3,
                expected,
                delta=1e-7,
            )
            self.assertIs(near, asset.near_products())
            for product in (asset.full, near):
                joined = native(append(product.body, [product.skin]))
                physical = native(product.physical.mesh, product.physical.vertex_roots)
                self.assertLess(
                    (joined - physical).volume() + (physical - joined).volume(), 1e-7
                )

    def test_near_front_mouths_are_open_and_remaining_front_uvs_are_metrical(self):
        for asset in self.assets:
            skin = asset.near_products().skin
            xyz = skin["vertices"][skin["faces"]]
            n = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
            n /= np.linalg.norm(n, axis=1, keepdims=True)
            for p in asset.dormers:
                frame = p.frame()
                mask, basis, _rect = selection(xyz, n, frame, p.base_m)
                local = (xyz[mask] - np.r_[p.a, p.base_m]) @ basis.T
                region = shapely.union_all(shapely.polygons(local[:, :, :2]))
                self.assertFalse(
                    region.covers(shapely.Point(p.width_m / 2, frame["height_m"] / 2))
                )
                uv = skin["uv"][skin["faces"][mask]] * [p.width_m, frame["height_m"]]
                for a, b in ((0, 1), (1, 2), (2, 0)):
                    np.testing.assert_allclose(
                        np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
                        np.linalg.norm(xyz[mask, a] - xyz[mask, b], axis=1),
                        rtol=0,
                        atol=1e-8,
                    )

    def test_source_programmes_keep_original_facade_access_and_are_view_independent(
        self,
    ):
        for row, neighbors in self.rows:
            v = view_for(row)
            _old, old, _before = build(ElevationFacades, row, neighbors, v)
            _mesh, info, owner = build(DormerFacades, row, neighbors, v)
            self.assertFalse(owner.rejected)
            for key in (
                "entrance_plan",
                "shopfront_plan",
                "balcony_plan",
                "window_bay_plan",
                "facade_programme",
                "facade_architecture_plan",
            ):
                self.assertEqual(untimed(old.get(key)), untimed(info.get(key)), key)
            far = replace(v, eye=v.target + (v.eye - v.target) * 10000)
            _far, remote, other = build(DormerFacades, row, neighbors, far)
            self.assertEqual(info["roof_dormer_plan"], remote["roof_dormer_plan"])
            self.assertEqual(remote["roof_dormer_product"], "full")
            self.assertIsNone(other.base.dormer_assets[row["id"]].near)

    def test_invalid_capacity_roofs_contacts_and_duplicate_sources_are_rejected(self):
        row, _neighbors = self.rows[0]
        asset = self.assets[0]
        c = choose(row, attributes(row["properties"]))
        for bad in (0, True, 65):
            with self.assertRaises(ValueError):
                plan(row, asset.original, c, maximum=bad)
        for bad in (replace(c, kind="flat"), replace(c, rise_m=2.0)):
            self.assertFalse(plan(row, asset.original, bad)[0])
        self.assertFalse(plan(row, asset.original, c, clear=lambda *_args: False)[0])
        with self.assertRaises(ValueError):
            union(asset.original, (asset.dormers[0],) * 2)
        with self.assertRaises(ValueError):
            union(asset.original, (replace(asset.dormers[0], cap_m=10000),))

    def test_existing_crown_fallback_keeps_all_previous_arrays_and_function_fields(
        self,
    ):
        from roof_crown_sources import sources as crowns

        row, neighbors = crowns()[0]
        v = view_for(row)
        old, first, _before = build(ElevationFacades, row, neighbors, v)
        new, second, owner = build(DormerFacades, row, neighbors, v)
        for key in old:
            np.testing.assert_array_equal(old[key], new[key])
        self.assertEqual(untimed(first), untimed(second))
        self.assertFalse(owner.base.dormer_assets)

    def test_roof_budget_exhaustion_keeps_the_original_source_exterior(self):
        row, neighbors = self.rows[0]
        v = view_for(row)
        old, first, _before = build(ElevationFacades, row, neighbors, v)
        with patch(
            "tile_dormers.prepare",
            side_effect=SkeletonBudgetExceeded("bounded roof work exhausted"),
        ):
            new, second, owner = build(DormerFacades, row, neighbors, v)
        for key in old:
            np.testing.assert_array_equal(old[key], new[key])
        self.assertEqual(untimed(first), untimed(second))
        self.assertNotIn("roof_dormer_plan", second)
        self.assertIn(row["id"], owner.base.failed)
        self.assertTrue(
            any("bounded roof work" in r["reason"] for r in owner.base.dormer_rejected)
        )

    def test_inherited_valid_roof_deviation_is_separate_from_new_union_error(self):
        from dormer_footprint import projected
        from dormer_sources import counterexamples

        for row in counterexamples():
            params = attributes(row["properties"])
            asset = prepare(row, params, choose(row, params))
            parent = shapely.from_wkb(asset.original.footprint_wkb)
            self.assertLessEqual(row["polygon"].symmetric_difference(parent).area, 1e-7)
            actual = projected(asset.full.physical.mesh)
            self.assertLessEqual(row["polygon"].symmetric_difference(actual).area, 1e-7)
            self.assertLessEqual(parent.symmetric_difference(actual).area, 1e-8)


if __name__ == "__main__":
    unittest.main()
