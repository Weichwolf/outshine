from types import SimpleNamespace
from unittest import TestCase

import numpy as np
from bridge_passages import FLOOR_ABOVE_WATER_M, water_clearance
from reference_scene import support_height
from river_control import RiverControl, control_surfaces
from shapely.geometry import GeometryCollection, LineString, Point, Polygon, box
from shore_grade import SHORE_REACH_M, shore_grade
from water_bed import nearshore_bed
from water_surface import WaterBody, clipped_water, water_bodies


class WaterSurfaceTests(TestCase):
    def test_gate_splits_one_id_and_keeps_grade_across_tile_edges(self):
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        control = RiverControl.from_recipe(
            {
                "water_class": "river",
                "water_id": 4,
                "plateaus_model_m": [161.0, 158.0],
                "overflow_run_m": 1.0,
                "section_lon_lat": [[16.3998, 48.2], [16.4002, 48.2]],
                "downstream_centerline_lon_lat": [[16.4, 48.2], [16.4, 48.19]],
                "station_km_and_adria_m": [[0.58, 157.9], [1.58, 157.5]],
            }
        )
        tags = {"class": "river", "id": 4}
        extent = box(-10, -1000, 10, 100)
        height = lambda xy: np.full(len(xy), -1.0)

        def solution(features):
            return tuple(
                water_bodies(
                    features,
                    extent,
                    np.array([[0, 0]]),
                    height,
                    height,
                    source=source,
                    controls=(control,),
                )
            )

        bodies = solution([("water", extent, tags)])
        self.assertEqual(len(bodies), 2)
        self.assertAlmostEqual(sum(body.footprint.area for body in bodies), extent.area)
        still = solution([("water", box(-10, -800, 10, -200), tags)])
        self.assertEqual(len(still), 1)
        low = next(body for body in bodies if body.profile is not None)
        near_xy = np.array([[0.0, -20], [0.0, -400], [0.0, -600]])
        z = low.heights(source, near_xy)
        self.assertTrue((np.diff(z) < 0).all())
        np.testing.assert_allclose(still[0].heights(source, near_xy[1:]), z[1:])

        ((_, north_patch),) = clipped_water(bodies, box(-10, -400, 10, -1))
        ((_, south_patch),) = clipped_water(bodies, box(-10, -800, 10, -400))
        a = support_height(low.mesh(source, north_patch), (0, -399.97)).max()
        b = support_height(low.mesh(source, south_patch), (0, -400.03)).max()
        self.assertLess(abs(a - b), 0.001)

        ((wall_material, wall, faces), (flow_material, spill, spill_faces)) = tuple(
            control_surfaces(source, bodies)
        )
        self.assertEqual(wall_material, "bridge deck")
        self.assertEqual(flow_material, "weir overflow")
        self.assertGreater(len(faces), 0)
        crest, toe = np.split(wall[:, 2], 2)
        np.testing.assert_allclose(crest - toe, 3.25, atol=1e-6)
        cascade = spill.reshape(-1, len(crest), 3)
        np.testing.assert_allclose(cascade[0], wall[: len(crest)], atol=1e-9)
        self.assertTrue((np.diff(cascade[:, :, 2], axis=0) < 0).all())
        self.assertTrue((np.diff(cascade[:, :, 1], axis=0) < 0).all())
        self.assertEqual(len(spill_faces), (len(cascade) - 1) * len(faces))
        np.testing.assert_allclose(
            cascade[-1, :, 2], low.heights(source, cascade[-1, :, :2]) - 0.08
        )
        floors = [(body.footprint, body) for body in bodies]
        lower = water_clearance(source, floors, np.array([[12, 5], [12, -100]]))
        self.assertAlmostEqual(lower[0], 1 + FLOOR_ABOVE_WATER_M)
        self.assertLess(lower[1], -2 + FLOOR_ABOVE_WATER_M)

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
        self.assertIsNone(nearshore_bed(grade, extent))

    def test_inferred_bed_fades_with_depth_and_leaves_quay_dark(self):
        water = (WaterBody(box(-40, -40, 0, 40), 0, water_class="lake"),)
        extent = box(-30, -30, 30, 30)
        grade = shore_grade(water, extent, box(-1, 10, 5, 20))
        bed = nearshore_bed(grade, extent)
        east, north, sx, sy = bed.transform
        h, w = bed.transmission.shape[:2]

        def texel(x, y):
            return bed.transmission[int(h * (y - north) * sy), int(w * (x - east) * sx)]

        bank = texel(-0.5, -10)
        channel = texel(-4.0, -10)
        self.assertTrue((bank > channel).all())
        self.assertLess(channel[0], channel[1])
        self.assertLess(channel[1], channel[2])
        np.testing.assert_array_equal(texel(-0.5, 15), [0, 0, 0])
        self.assertTrue((texel(-0.5, 29) < 0.08).all())

    def test_line_fragments_do_not_disable_paved_shore_protection(self):
        source = SimpleNamespace(
            lon=16.4, lat=48.2, metres=np.array([74300, 111180]), datum=160
        )
        water = (WaterBody(box(-40, -20, 0, 20), 0.0),)
        extent = box(-35, -15, 20, 15)
        road = box(7, -2, 14, 2)
        fragment = LineString([(2, 6), (9, 8)])
        mixed = shore_grade(water, extent, GeometryCollection([road, fragment]))
        paved = shore_grade(water, extent, road)
        sites = np.array([[6.99, 0, 3], [7, 0, 3], [7.01, 0, 3], [5.5, 7, 3]])
        z = mixed.heights(source, sites)
        np.testing.assert_allclose(z, paved.heights(source, sites))
        self.assertLess(abs(z[0] - z[1]), 0.001)
        self.assertEqual(z[2], 3)
        self.assertLess(z[3], 2)

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
