import unittest

import numpy as np
from shapely.geometry import Polygon, box

from outshine_ref.buildings.hlod import (
    Building, bake, block_hulls, combine, extrusions, partition, remesh,
)


class BuildingHlod(unittest.TestCase):
    def test_extruded_courtyard_has_no_roof_over_its_hole(self):
        p = Polygon([(0, 0), (20, 0), (20, 20), (0, 20)],
                    [[(5, 5), (5, 15), (15, 15), (15, 5)]])
        building = Building(p, 12, 9, 3, (.5, .4, .3))
        result = extrusions([building])
        faces = result.vertices[result.triangles]
        projected = np.abs(np.cross(faces[:, 1] - faces[:, 0],
                                   faces[:, 2] - faces[:, 0])[:, 2]) / 2
        self.assertAlmostEqual(projected.sum(), p.area)
        for face, area in zip(faces, projected):
            if area > 0:
                self.assertTrue(p.covers(Polygon(face[:, :2])))
        self.assertEqual(result.vertices[:, 2].max(), 21)
        self.assertEqual(result.vertices[:, 2].min(), 12)

    def test_crossing_roof_keeps_its_ridge_after_tile_clipping(self):
        building = Building(box(2, 2, 18, 10), 20, 10, 3, (.4, .4, .4))
        tiles = partition([building], (0, 0, 20, 20), 2)
        result = combine(extrusions(members) for _, members in tiles)
        whole = extrusions([building])
        for point in whole.vertices:
            self.assertTrue(np.any(np.all(np.isclose(result.vertices, point), axis=1)))
        on_split = result.vertices[np.isclose(result.vertices[:, 0], 10)]
        self.assertEqual(on_split[:, 2].max(), 30)
        self.assertAlmostEqual(sum(b.footprint.area for _, bs in tiles for b in bs), 128)

    def test_coarse_bake_keeps_thin_tower_and_stays_in_tile(self):
        building = Building(box(15.9, 15.9, 16.1, 16.1), 5, 200, 0, (.5, .5, .5))
        ground = np.full((65, 65), 5)
        field = bake([building], (0, 0, 64, 64), ground)
        surface = remesh(field)
        self.assertEqual(surface.vertices[:, 2].max(), 205)
        self.assertLessEqual(len(surface.triangles), 20000)
        self.assertTrue(np.all(surface.vertices[:, :2] >= 0))
        self.assertTrue(np.all(surface.vertices[:, :2] <= 64))
        hull = block_hulls([building], (0, 0, 64, 64))
        self.assertEqual(hull.vertices[:, 2].max(), 205)
        flat = remesh(field, heights=ground)
        self.assertTrue(np.all(flat.vertices[:, 2] == 5))

    def test_area_weighted_block_height_exposes_tower_loss(self):
        a = Building(box(0, 0, 10, 10), 5, 100, 0, (.5, .4, .3))
        b = Building(box(12, 0, 22, 10), 5, 10, 0, (.3, .4, .5))
        result = block_hulls([a, b], (0, 0, 22, 10))
        self.assertEqual(result.vertices[:, 2].max(), 60)
        self.assertGreater(a.base + a.height - result.vertices[:, 2].max(), 4)
