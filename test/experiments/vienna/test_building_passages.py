import unittest

import numpy as np
import shapely
from building_passages import PORTAL_WIDTH_M, BuildingPassage, building_passages
from passage_light import PORTAL_MARGIN_M, passage_probe_regions
from reference_scene import support_height
from scene_mesh import assemble

FOOTWAY = {"class": "path", "subclass": "footway", "surface": "paving_stones"}
TUNNEL = FOOTWAY | {"brunnel": "tunnel", "layer": -1}
HOUSE = {"render_height": 12, "render_min_height": 0}


def line(*points):
    return shapely.LineString(points)


class Survey:
    def __init__(self, features):
        self.features = features

    def height(self, points):
        return np.asarray(points)[:, 1] * 0.04


class BuildingPassageTest(unittest.TestCase):
    def setUp(self):
        self.block = shapely.box(-4, -5, 4, 5)
        self.tunnel = line((0, -5), (0, 5))
        self.walks = [
            (line((0, -9), (0, -5)), FOOTWAY),
            (line((0, 5), (0, 9)), FOOTWAY),
        ]

    def test_covered_walk_connects_at_grade_approaches(self):
        features = [(self.block, HOUSE), *self.walks, (self.tunnel, TUNNEL)]
        source = Survey(
            [
                ("building" if props == HOUSE else "transportation", geom, props)
                for geom, props in features
            ]
        )
        meshes = assemble(source, 16)
        (passage,) = meshes.covered_walks
        self.assertLess(passage.floor_m, 0)
        for y in range(-8, 9):
            point = (0.0, y + 0.13)
            pavement = support_height(meshes["paving"], point)
            self.assertGreater(len(pavement), 0, point)
            if abs(point[1]) < 5:
                soffit = support_height(meshes["wall"], point)
                self.assertGreater(len(soffit), 0, point)
                self.assertGreater(max(soffit) - max(pavement), 2.9)
                self.assertLess(max(soffit) - max(pavement), 3.5)
                spring = max(support_height(meshes["wall"], (1.15, point[1])))
                self.assertLess(spring, max(soffit) - 0.4)
                self.assertGreater(spring, max(pavement) + 2)

        points, faces = meshes["wall"]
        walls = np.asarray(points)[faces]
        at_eye = walls[:, :, 2].max(axis=1) > 1.7
        at_eye &= walls[:, :, 2].min(axis=1) < 1.7
        mask = np.ptp(walls[:, :, 1], axis=1) < 1e-7
        blocked = (walls[:, :, 0].min(axis=1) < 0) & (walls[:, :, 0].max(axis=1) > 0)
        self.assertFalse((at_eye & mask & blocked).any())

    def test_irradiance_region_covers_linked_sloping_walks(self):
        walks = []
        for x, floor, crown in ((0, -1, 3), (13, 1, 4.8), (100, 0, 3)):
            route = line((x, 0), (x + 9, 4))
            walks.append(
                BuildingPassage(
                    route, route.buffer(1.2, cap_style="flat"), {}, floor, crown
                )
            )
        ((center, radii, angle),) = passage_probe_regions(walks, (0, 0))
        self.assertAlmostEqual(center[2] - radii[2], -1.8)
        self.assertAlmostEqual(center[2] + radii[2], 7.1)
        planar_axes = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        for passage in walks[:2]:
            ring = passage.corridor.buffer(PORTAL_MARGIN_M, join_style="mitre").exterior
            projected = (np.array(ring.coords) - center[:2]) @ planar_axes
            self.assertTrue((abs(projected) <= np.asarray(radii[:2]) + 1e-6).all())
        self.assertEqual(list(passage_probe_regions(walks, (0, 100))), [])

    def test_tunnels_without_two_exposed_pedestrian_approaches_stay_buried(self):
        extent = shapely.box(-30, -30, 30, 30)
        independent = shapely.affinity.translate(self.tunnel, xoff=12)
        under_second_block = shapely.box(-4, 4, 4, 8)
        cases = [
            ([(self.tunnel, TUNNEL), self.walks[0]], [self.block]),
            ([(independent, TUNNEL), *self.walks], [self.block]),
            ([(self.tunnel, TUNNEL), *self.walks], [self.block, under_second_block]),
        ]
        for transport, footprints in cases:
            with self.subTest(transport=transport):
                self.assertFalse(
                    building_passages(transport, footprints, extent, Survey([]).height)
                )

    def test_stone_surround_follows_an_oblique_wall_without_narrowing_the_walk(self):
        features = [
            ("building", self.block, HOUSE),
            ("transportation", line((-1, -9), (-1, -5)), FOOTWAY),
            ("transportation", line((-1, -5), (1, 5)), TUNNEL),
            ("transportation", line((1, 5), (1, 9)), FOOTWAY),
        ]
        points, faces = assemble(Survey(features), 16)["portal stone"]
        points = np.asarray(points)
        self.assertLess(points[:, 1].min(), -5.08)
        self.assertGreater(points[:, 1].max(), 5.08)

        triangles = points[faces]
        at_eye = (triangles[:, :, 2].min(axis=1) <= 1.7) & (
            triangles[:, :, 2].max(axis=1) >= 1.7
        )
        axis = np.array([-5.0, 1.0]) / np.sqrt(26.0)
        spans = triangles[at_eye, :, :2] @ axis
        self.assertTrue(
            np.all(
                (spans.min(axis=1) >= PORTAL_WIDTH_M / 2 - 1e-7)
                | (spans.max(axis=1) <= -PORTAL_WIDTH_M / 2 + 1e-7)
            )
        )
