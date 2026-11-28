import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from building_roofs import compile_gable, gable_plan
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

    def test_roofs_keep_the_flat_cap_when_adjacent_parts_share_roof_height(self):
        a, b = shapely.box(0, 0, 20, 8), shapely.box(20, 0, 38, 8)

        def build(*parts):
            return assemble(
                SimpleNamespace(
                    features=[("building", p, {"render_height": 12}) for p in parts],
                    height=lambda xy: np.zeros(len(xy)),
                ),
                45,
            )

        self.assertIn("roof tile", build(a))
        attached = build(a, b)
        self.assertNotIn("roof tile", attached)
        self.assertTrue(np.allclose(np.asarray(attached["roof"][0])[:, 2], 12.5))
