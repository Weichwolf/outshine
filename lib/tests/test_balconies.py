import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.balconies import (
    BalconyPlan,
    balcony_plates,
    balcony_surface,
)


class Balconies(unittest.TestCase):
    def test_render_plates_bound_removed_boundary_positions_and_have_separate_capacity(
        self,
    ):
        for width in (1.8, 2.8, 4.0):
            plan = BalconyPlan(width)
            full, plates = balcony_surface(plan), balcony_plates(plan)
            self.assertEqual(len(plates["faces"]), 16)
            quads = plates["vertices"][plates["faces"]].reshape(8, 6, 3)
            low, high = quads.min(axis=1), quads.max(axis=1)
            weights = np.array(
                [
                    (i / 8, j / 8, 1 - (i + j) / 8)
                    for i in range(9)
                    for j in range(9 - i)
                ]
            )
            points = np.einsum(
                "ij,tjk->tik", weights, full["vertices"][full["faces"]]
            ).reshape(-1, 3)
            gaps = np.maximum(
                np.maximum(low[None] - points[:, None], points[:, None] - high[None]), 0
            )
            self.assertLessEqual(
                np.linalg.norm(gaps, axis=2).min(axis=1).max(),
                max(plan.guard_m, plan.slab_m / 2) + 1e-10,
            )
            self.assertEqual(set(plates["roles"]), {0, 1})
        with self.assertRaisesRegex(ValueError, "capacity"):
            balcony_plates(plan, max_triangles=15)

    def test_closed_oriented_union_volume_and_analytic_sections(self):
        import manifold3d as manifold
        import shapely
        from outshine_ref.buildings.modelling.mesh_fans import connected_fans

        for width in (1.8, 2.8, 4.0):
            plan = BalconyPlan(width)
            result = balcony_surface(plan)
            properties, faces, duplicates, targets = connected_fans(
                result | {"uv": np.zeros((len(result["vertices"]), 2))},
                np.arange(len(result["faces"])),
            )
            solid = manifold.Manifold(
                manifold.Mesh64(
                    np.ascontiguousarray(properties[:, :3]),
                    faces,
                    merge_from_vert=duplicates,
                    merge_to_vert=targets,
                )
            )
            self.assertEqual(solid.status(), manifold.Error.NoError)
            self.assertAlmostEqual(solid.volume(), plan.volume_m3, places=10)
            for z in (-0.08, 0.4, 1.09):
                rings = [shapely.Polygon(r) for r in solid.slice(z).to_polygons()]
                area = sum(p.area for p in rings)
                expected = (
                    width * (plan.depth_m + plan.back_m)
                    if z < 0
                    else width * plan.depth_m - (width - 0.2) * (plan.depth_m - 0.1)
                )
                self.assertAlmostEqual(area, expected, places=7)
            self.assertEqual(set(result["roles"]), {0, 1})

    def test_declared_dimensions_and_capacity_before_expansion(self):
        for args in ((True,), (float("nan"),), (0.1,), (2.8, 0.05)):
            with self.assertRaises(ValueError):
                BalconyPlan(*args)
        with (
            patch(
                "outshine_ref.buildings.balconies.volumes",
                side_effect=AssertionError("expanded"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            balcony_surface(BalconyPlan(2.8), max_triangles=47)

    def test_owned_buffers_and_disjoint_part_volume(self):
        plan = BalconyPlan(2.8)
        union, parts = balcony_surface(plan), balcony_surface(plan, method="parts")
        for result in (union, parts):
            points = result["vertices"][result["faces"]]
            volume = (
                np.einsum(
                    "ij,ij->i", points[:, 0], np.cross(points[:, 1], points[:, 2])
                ).sum()
                / 6
            )
            self.assertAlmostEqual(volume, plan.volume_m3, places=10)
            self.assertEqual(len(result["faces"]), len(result["roles"]))
        union["vertices"][:] = 123
        self.assertTrue(np.max(abs(balcony_surface(plan)["vertices"])) < 2)
