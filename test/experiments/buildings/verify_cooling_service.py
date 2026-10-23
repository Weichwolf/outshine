"""Real closed components, continuous body space, source bounds and original inlet paths."""

import unittest
from dataclasses import replace

import manifold3d as manifold
import numpy as np
import shapely
from cooling_service_mesh import components, world_points
from cooling_service_plan import plan
from cooling_tower_details import refine
from cooling_tower_mesh import components as tower_components
from cooling_tower_probe import sources
from structural_shell_geometry import native
from verify_cooling_tower import passage
from verify_recipe_halls import mesh_equal


class CoolingService(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows, cls.receipt, cls.view, cls.cases = sources()
        cls.access = [plan(tower) for _row, tower in cls.cases]
        cls.parts = [tower_components(a.tower) | components(a) for a in cls.access]
        cls.solids = [
            manifold.Manifold.batch_boolean(
                [native(p) for p in parts.values()], manifold.OpType.Add
            )
            for parts in cls.parts
        ]

    def test_real_service_connects_and_preserves_footprint_and_height(self):
        self.assertEqual(len(self.cases), 2)
        for (row, tower), access, parts, solid in zip(
            self.cases, self.access, self.parts, self.solids, strict=True
        ):
            self.assertEqual(access, plan(tower))
            self.assertEqual(len(solid.decompose()), 1)
            self.assertGreater(solid.volume(), 0)
            for mesh in parts.values():
                self.assertGreater(native(mesh).volume(), 0)
                self.assertTrue(
                    shapely.covers(
                        row["polygon"].buffer(1e-9),
                        shapely.points(mesh["vertices"][:, :2]),
                    ).all()
                )
                self.assertGreaterEqual(
                    mesh["vertices"][:, 2].min(), tower.source.knots[0][0]
                )
                self.assertLessEqual(
                    mesh["vertices"][:, 2].max(), tower.source.knots[-1][0]
                )
                self.assertTrue(
                    all(
                        not a.flags.writeable and np.isfinite(a).all()
                        for a in mesh.values()
                    )
                )

    def test_actual_climb_and_step_off_are_continuously_free_and_floor_supports_arrival(
        self,
    ):
        for access, solid in zip(self.access, self.solids, strict=True):
            for mesh in components(access, clearance=True).values():
                swept = native(mesh)
                self.assertGreater(swept.volume(), 1)
                self.assertEqual((solid ^ swept).volume(), 0)
            end = world_points(access, access.route)[-1]
            floor = components(access)[
                access.tower.source.identifier + ":service:floor"
            ]
            hits = native(floor).ray_cast(end + [0, 0, 0.5], end - [0, 0, 0.5])
            self.assertTrue(hits)
            self.assertAlmostEqual(hits[0].distance, 0.52, places=8)

    def test_all_original_intake_paths_remain_free(self):
        for access, solid in zip(self.access, self.solids, strict=True):
            for bay in range(access.tower.bays):
                self.assertEqual((solid ^ passage(access.tower, bay)).volume(), 0)

    def test_different_bay_and_coarse_product_preserve_connection_and_clearance(self):
        tower = self.cases[0][1]
        for bay, error in ((1, 0.05), (tower.bays // 3, 1.0)):
            access = plan(tower, bay=bay)
            parts = tower_components(tower, error_m=error) | components(access)
            solid = manifold.Manifold.batch_boolean(
                [native(p) for p in parts.values()], manifold.OpType.Add
            )
            self.assertEqual(len(solid.decompose()), 1)
            for mesh in components(access, clearance=True).values():
                self.assertEqual((solid ^ native(mesh)).volume(), 0)

    def test_default_and_declined_access_keep_complete_previous_product(self):
        row, tower = self.cases[0]
        info = {"source_kind": "cooling_tower"}
        before = refine(row, tower.source, info.copy(), 0.2)
        disabled = refine(row, tower.source, info.copy(), 0.2, service_access=False)
        declined = refine(
            row, tower.source, info.copy(), 0.2, service_access=True, service_bay=-1
        )
        mesh_equal(before[0], disabled[0])
        mesh_equal(before[0], declined[0])
        self.assertIn("service_access_declined", declined[1])
        after = refine(row, tower.source, info.copy(), 0.2, service_access=True)
        self.assertEqual(after[1]["service_access"]["source_id"], row["id"])
        self.assertEqual(after[1]["profile_knots"], tower.source.knots)

    def test_source_bounds_and_fit_rejections_are_explicit(self):
        tower = self.cases[0][1]
        for bay in (-1, tower.bays, True):
            with self.assertRaises(ValueError):
                plan(tower, bay=bay)
        collector = replace(
            tower.collector,
            knots=(
                (7, tower.source.knots[0][1] - 0.3, 40),
                (7.6, tower.source.knots[0][1] - 0.3, 40),
            ),
        )
        with self.assertRaisesRegex(ValueError, "no bounded service landing"):
            plan(replace(tower, collector=collector))


if __name__ == "__main__":
    unittest.main()
