from types import SimpleNamespace
from unittest import TestCase

import numpy as np
from reference_scene import support_height
from shapely.geometry import LineString, Point, Polygon, box
from shore_grade import SHORE_REACH_M, shore_grade
from water_surface import WaterBody, clipped_water, water_bodies


class WaterSurfaceTests(TestCase):
    def test_natural_shore_meets_water_and_preserves_pavement(self):
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        body = WaterBody(box(-40, -20, 0, 20), 0.0)
        extent = box(-35, -15, 20, 15)
        east = np.linspace(0, SHORE_REACH_M + 1, 61)
        ground = np.column_stack((east, np.zeros(len(east)), np.full(len(east), 3.0)))
        grade = shore_grade((body,), extent, Polygon())
        z = grade.heights(source, ground)
        self.assertAlmostEqual(z[0], body.heights(source, ground[:1, :2])[0])
        self.assertTrue((np.diff(z) >= 0).all())
        self.assertLess(np.max(np.diff(z) / np.diff(east)), 0.35)
        self.assertEqual(z[-1], 3.0)

        paved = shore_grade((body,), extent, box(7, -2, 14, 2))
        pz = paved.heights(source, ground)
        self.assertAlmostEqual(pz[0], z[0])
        self.assertTrue((pz[east >= 7] == 3.0).all())

    def test_grade_excludes_water_to_water_reach_boundary(self):
        extent = box(-30, -30, 30, 30)
        west = WaterBody(box(-100, -100, 0, 100), 0.0)
        east = WaterBody(box(0, -100, 100, 100), 4.0)
        grade = shore_grade((west, east), extent, Polygon())
        self.assertEqual(grade.contacts, ())

    def test_oblique_bank_survives_clipping_at_a_patch_boundary(self):
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        footprint = Polygon([(-80, -80), (80, -23), (80, 60), (-80, 97)])
        water = (WaterBody(footprint, 0.0),)
        near = shore_grade(water, box(-50, -80, 17.8, 80), Polygon())
        far = shore_grade(water, box(17.8, -80, 77, 80), Polygon())
        ground = np.array([[17.8, -48.0, 3.0]])
        a = near.heights(source, ground)
        b = far.heights(source, ground)
        self.assertLess(a[0], 1.0)
        np.testing.assert_allclose(a, b, atol=2e-6)

    def test_neighboring_grade_preserves_paved_contact_at_its_boundary(self):
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        water = (WaterBody(box(-40, -60, 0, 60), 0.0),)
        near = shore_grade(water, box(-30, -30, 30, 0), box(4, -5, 9, 0))
        seam = LineString([(-30, 0), (30, 0)])
        far = shore_grade(
            water, box(-30, 0, 30, 30), Polygon(), neighbor=(near.protection, seam)
        )
        plain = shore_grade(water, box(-30, 0, 30, 30), Polygon())
        at_seam = np.array([[6.0, 0.0, 3.0]])
        np.testing.assert_allclose(
            far.heights(source, at_seam), near.heights(source, at_seam)
        )
        beyond = np.array([[6.0, SHORE_REACH_M + 1, 3.0]])
        np.testing.assert_allclose(
            far.heights(source, beyond), plain.heights(source, beyond)
        )

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
