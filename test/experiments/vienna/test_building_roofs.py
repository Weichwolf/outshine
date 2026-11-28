import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from building_roofs import compile_gable, gable_plan, plan_roofs
from scene_mesh import assemble


class BuildingRoofTest(unittest.TestCase):
    def test_gable_closes_ends_within_mapped_height(self):
        footprint = shapely.box(0, 0, 20, 8)
        plan = gable_plan(
            footprint, 0.5, 12.5, {"roof:shape": "gabled", "roof:height": 2.5}
        )
        roof, ends = compile_gable(footprint, plan)
        self.assertAlmostEqual(float(roof[:, 2].max()), 12.5)
        self.assertAlmostEqual(float(ends[:, 2].min()), 10)
        self.assertAlmostEqual(float(ends[:, 2].max()), 12.5)
        projected = shapely.polygons(roof.reshape(-1, 3, 3)[:, :, :2])
        self.assertLess(
            shapely.union_all(projected).symmetric_difference(footprint).area, 1e-8
        )
        self.assertIsNone(gable_plan(footprint, 0, 12, {"roof:shape": "flat"}))
        self.assertIsNone(
            gable_plan(footprint.difference(shapely.box(1, 1, 4, 4)), 0, 12, {})
        )

    def test_adjacent_parts_share_a_ridge_and_eave(self):
        a, b = shapely.box(0, 0, 20, 8), shapely.box(20, 0, 38, 8)

        def build(*parts):
            return assemble(
                SimpleNamespace(
                    features=[("building", p, {"render_height": 12}) for p in parts],
                    height=lambda xy: np.zeros(len(xy)),
                ),
                45,
            )

        attached = build(a, b)
        roof = np.asarray(attached["roof tile"][0])
        self.assertNotIn("roof", attached)
        self.assertAlmostEqual(float(roof[:, 2].max()), 12.5)
        projected = shapely.polygons(roof.reshape(-1, 3, 3)[:, :, :2])
        self.assertLess(
            shapely.union_all(projected).symmetric_difference(a | b).area, 1e-8
        )
        eaves, roofs = plan_roofs(
            [a, b], [0.5, 0.5], [12.5, 12.5], [{}, {}], [True, True]
        )
        self.assertEqual(len(roofs), 1)
        self.assertEqual(eaves[0], eaves[1])
        self.assertFalse(np.isclose(roofs[0][1].ends[:, 0], 20).any())

    def test_hip_over_a_courtyard_leaves_its_eaves_and_open_sky(self):
        courtyard = shapely.box(8, 8, 28, 34)
        footprint = shapely.box(0, 0, 36, 42).difference(courtyard)
        eaves, roofs = plan_roofs([footprint], [0], [15], [{}], [True])
        self.assertEqual(len(roofs), 1)
        roof = roofs[0][1].surface
        projection = shapely.union_all(
            shapely.polygons(roof.reshape(-1, 3, 3)[:, :, :2])
        )
        self.assertLess(projection.symmetric_difference(footprint).area, 1e-8)
        self.assertLess(projection.intersection(courtyard).area, 1e-8)
        self.assertAlmostEqual(float(roof[:, 2].max()), 15)
        boundary = shapely.dwithin(
            shapely.points(roof[:, :2]), courtyard.boundary, 1e-7
        )
        self.assertTrue(boundary.any())
        self.assertTrue(np.allclose(roof[boundary, 2], eaves[0]))

    def test_incompatible_parts_keep_the_flat_part_and_existing_partition(self):
        a, b = shapely.box(0, 0, 20, 8), shapely.box(20, 0, 38, 8)
        eaves, roofs = plan_roofs(
            [a, b], [0, 0], [12, 12], [{}, {"roof:shape": "flat"}], [True, True]
        )
        self.assertEqual(len(roofs), 1)
        self.assertEqual(eaves[1], 12)
        self.assertLessEqual(float(roofs[0][1].surface[:, 0].max()), 20)

    def test_tall_facade_projection_does_not_flatten_the_main_roof(self):
        wing = shapely.box(0, 0, 26, 10)
        projection = shapely.box(10, -1, 16, 0.1)
        eaves, roofs = plan_roofs(
            [wing, projection], [0, 0], [15, 18], [{}, {}], [True, True]
        )
        self.assertEqual([index for index, _ in roofs], [0])
        self.assertLess(eaves[0], 15)
        self.assertEqual(eaves[1], 18)
