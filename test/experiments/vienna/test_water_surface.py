from types import SimpleNamespace
from unittest import TestCase

import numpy as np
from reference_scene import support_height
from shapely.geometry import Point, box
from water_surface import clipped_water, water_bodies


class WaterSurfaceTests(TestCase):
    def test_one_reach_shares_a_level_across_the_mesh_boundary(self):
        local = box(-250, -250, 250, 250)
        regional = box(250, -250, 750, 250)
        tags = {"class": "river", "id": 4}
        features = [
            ("water", box(50, -60, 300, 50), tags),
            ("water", box(200, -60, 550, 50), tags),
        ]
        samples = np.array([[100, 0], [200, 0]])
        height = lambda xy: np.full(len(xy), 2.0)
        far = lambda xy: np.full(len(xy), 6.0)
        bodies = tuple(water_bodies(features, local, samples, height, far))
        self.assertEqual(len(bodies), 1)
        self.assertEqual(bodies[0].level, 2.0)
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        ((body, left),) = clipped_water(bodies, local)
        ((_, right),) = clipped_water(bodies, regional)
        a, b = body.mesh(source, left), body.mesh(source, right)
        for y in np.linspace(-58, 48, 19):
            here = support_height(a, (249.98, y)).max()
            there = support_height(b, (250.02, y)).max()
            self.assertLess(abs(here - there), 0.0001)

    def test_touching_mapped_reaches_keep_independent_levels(self):
        features = [
            ("water", box(0, 0, 10, 15), {"class": "river", "id": 1}),
            ("water", box(10, 0, 20, 15), {"class": "river", "id": 2}),
            ("water", box(0, 20, 10, 35), {"class": "river", "id": 1}),
            ("water", box(8, 3, 12, 12), {"class": "river", "id": 3}),
        ]
        domain = box(-1, -1, 21, 36)
        samples = np.array([[5, 6], [15, 6], [5, 26]])
        height = lambda xy: np.where(xy[:, 1] > 20, 8, np.where(xy[:, 0] < 10, 4, 1))
        bodies = tuple(water_bodies(features, domain, samples, height, height))
        self.assertEqual(len(bodies), 3)
        for x, y, level in [(9, 7, 4), (11, 7, 1), (5, 26, 8)]:
            self.assertEqual(
                [b.level for b in bodies if b.footprint.contains(Point(x, y))], [level]
            )
