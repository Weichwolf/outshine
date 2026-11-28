import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.buildings.building_foundations import BuildingStamps, building_foundations
from outshine.world.mesh import assemble
from outshine.world.scene import support_height


class BuildingFoundationTest(unittest.TestCase):
    def test_cut_fill_tapers_outside_the_building_and_reserves_the_street(self):
        footprint = shapely.box(0, 0, 24, 24).difference(shapely.box(4, 4, 20, 20))
        (foundation,) = building_foundations([footprint], lambda xy: xy[:, 0] * 0.04)
        reserve = shapely.box(-1, -1, 1, 1)
        ground = np.array(
            [
                [0, 0, 0],
                [2, 3, 0.08],
                [12, 12, 0.48],
                [-2, 2, 0],
                [-20, 2, 0],
                [1, 2, 0.04],
            ]
        )
        placed = BuildingStamps([foundation], reserve).heights(ground)
        self.assertAlmostEqual(placed[0], ground[0, 2])
        self.assertAlmostEqual(placed[1], foundation.platform_m)
        self.assertAlmostEqual(placed[2], ground[2, 2])
        self.assertGreater(placed[3], 0)
        self.assertLess(placed[3], foundation.platform_m)
        self.assertAlmostEqual(placed[4], ground[4, 2])
        self.assertGreater(placed[5], ground[5, 2])
        self.assertLess(placed[5], foundation.platform_m)

    def test_ground_and_plinth_share_the_building_site(self):
        footprint = shapely.box(-4, -3, 4, 3)
        source = SimpleNamespace(
            height=lambda xy: 0.1 * xy[:, 0],
            features=[("building", footprint, {"render_height": 5})],
        )
        meshes = assemble(source, 10)
        self.assertAlmostEqual(support_height(meshes["ground"]).max(), 0.4)
        self.assertAlmostEqual(np.max(np.array(meshes["foundation"][0])[:, 2]), 0.9)
        self.assertAlmostEqual(np.min(np.array(meshes["roof"][0])[:, 2]), 5.9)

    def test_steep_lot_reaches_grade_with_a_retained_plinth(self):
        footprint = shapely.box(-4, -3, 4, 3)
        (foundation,) = building_foundations([footprint], lambda xy: 0.6 * xy[:, 0])
        ground = np.array([[-4, 0, -2.4], [0, 0, 0], [4, 0, 2.4]])
        stamps = BuildingStamps([foundation], shapely.Polygon())
        np.testing.assert_array_equal(stamps.heights(ground), ground[:, 2])
        self.assertAlmostEqual(foundation.seat_m, 0)
        self.assertLess(foundation.sole_m, ground[:, 2].min())
