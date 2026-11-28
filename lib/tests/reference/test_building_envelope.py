import unittest

import numpy as np
import shapely
from outshine.buildings.building_envelope import (
    BuildingVolume,
    building_neighbours,
    exposed_cap,
    exposed_wall,
)
from outshine.buildings.facade import FacadeView, draw_facade
from outshine.world.mesh import Meshes
from shapely.geometry import box


class BuildingEnvelopeTest(unittest.TestCase):
    def test_coplanar_parts_share_one_owner_and_internal_walls_are_removed(self):
        body = box(0, 0, 12, 8)
        annex = box(4, 0, 8, 3)
        volumes = ([BuildingVolume(body, 0, 10)], [BuildingVolume(annex, 0, 6)])
        own, other = building_neighbours([body, annex], volumes)

        front = exposed_wall(np.array([0, 0]), np.array([12, 0]), 0, 10, own)
        expected = box(0, 0, 12, 10).difference(box(4, 0, 8, 6))
        self.assertTrue(front.equals(expected))
        self.assertIsNone(exposed_wall(np.array([4, 0]), np.array([8, 0]), 0, 6, other))
        back = exposed_wall(np.array([8, 3]), np.array([4, 3]), 0, 6, other)
        self.assertTrue(back.is_empty)
        self.assertTrue(exposed_cap(annex, 6, other).is_empty)

    def test_stacked_prisms_discard_buried_caps_and_retain_the_overhang_soffit(self):
        lower = box(0, 0, 6, 8)
        raised = box(3, 0, 9, 8)
        slabs = ([BuildingVolume(lower, 0, 5)], [BuildingVolume(raised, 5, 8)])
        low, high = building_neighbours([lower, raised], slabs)
        self.assertTrue(exposed_cap(lower, 5, low).equals(box(0, 0, 3, 8)))
        self.assertTrue(
            exposed_cap(raised, 5, high, underside=True).equals(box(6, 0, 9, 8))
        )

    def test_clipping_keeps_intact_bays_instead_of_leaving_window_fragments(self):
        a, b = np.array([0, 0]), np.array([12, 0])
        near = FacadeView((6, -8))
        block = BuildingVolume(box(1.9, -2, 2.2, 1), 0, 2.2)
        uncovered = exposed_wall(a, b, 0, 6.4, [(block, False)])
        whole, clipped = Meshes(), Meshes()
        draw_facade(whole, a, b, 0, 6.4, near, grounded=False, bays=True)
        draw_facade(
            clipped, a, b, 0, 6.4, near, grounded=False, bays=True, exposed=uncovered
        )
        original = np.asarray(whole["glass"][0])
        expected = original[(original[:, 0] > 3.0) | (original[:, 2] > 3.2)]
        actual = np.asarray(clipped["glass"][0])
        self.assertGreater(len(actual), 0)
        np.testing.assert_allclose(
            np.unique(actual, axis=0), np.unique(expected, axis=0)
        )
        trim = np.asarray(clipped["window sill"][0])
        self.assertFalse(np.any((trim[:, 0] <= 2.2) & (trim[:, 2] <= 2.2)))

        points, indices = clipped["wall"]
        faces = np.asarray(points)[indices]
        planar = np.all(faces[:, :, 1] == 0, axis=1)
        areas = shapely.union_all(shapely.polygons(faces[planar][:, :, [0, 2]]))
        self.assertTrue(areas.difference(uncovered).is_empty)
        self.assertTrue(areas.covers(shapely.Point(1.5, 1.5)))
