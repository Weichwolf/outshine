import unittest
from types import SimpleNamespace

import numpy as np
from outshine.contracts import Mesh
from outshine.geometry.sampling import SurfaceSampler
from outshine.terrain.ground_surface import at_grade_routes
from outshine.transport.bridge_passages import (
    CLEARANCE_M,
    FLOOR_ABOVE_WATER_M,
    MAXIMUM_GRADE,
    Passage,
)
from outshine.transport.bridges import WaterCrossing, slab
from outshine.transport.water_span import WaterSpan
from outshine.world.mesh import Meshes, assemble, bank_protection
from outshine.world.scene import support_height
from shapely.geometry import LineString, Point, Polygon, box


class BridgePassageTest(unittest.TestCase):
    def test_closed_slab_points_outward_with_reversed_rings(self):
        polygon = Polygon(
            [(0, 0), (0, 8), (8, 8), (8, 0)],
            holes=[[(2, 2), (6, 2), (6, 6), (2, 6)]],
        )
        meshes = Meshes()
        slab(meshes, "bridge deck", polygon, lambda xy: 5 + 0.03 * xy[:, 0], 1)
        vertices, faces = meshes["bridge deck"]
        triangles = np.asarray(vertices)[faces]
        normals = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        sides = abs(normals[:, 2]) < 1e-8
        self.assertEqual(np.count_nonzero(sides), 16)
        for centre, normal in zip(
            triangles[sides].mean(axis=1), normals[sides], strict=True
        ):
            outward = 0.01 * normal[:2] / np.linalg.norm(normal[:2])
            self.assertTrue(polygon.contains(Point(centre[:2] - outward)))
            self.assertFalse(polygon.covers(Point(centre[:2] + outward)))

    def test_bank_grading_protects_an_unpaved_path_beside_the_opening(self):
        extent = box(-10, -10, 10, 10)
        opening = box(-1, -1, 1, 1)
        path = LineString([(0, 0), (5, 0)])
        tags = {"class": "path", "surface": "compacted"}
        source = SimpleNamespace(features=[("transportation", path, tags)])
        roads = at_grade_routes([(path, tags)], extent)

        protected = bank_protection(source, extent, Polygon(), opening, roads, [], [])

        self.assertTrue(protected.covers(Point(3, 0)))
        self.assertFalse(protected.covers(Point(0, 0)))

    def test_path_floor_prefers_the_containing_face_between_close_cut_edges(self):
        xs = np.array([0.0, 1.0, 1.0000015, 2.0])
        xy = np.stack(np.meshgrid(xs, [0.0, 1.0]), axis=-1).reshape(-1, 2)
        triangles = (np.arange(3)[:, None] + [0, 1, 5, 0, 5, 4]).reshape(-1, 3)
        mesh = Mesh(np.c_[xy, 0.02 * xy[:, 0]], triangles)
        sampler = SurfaceSampler(mesh, xy_tolerance_m=2e-6, height_tolerance_m=3e-6)
        passage = Passage(SimpleNamespace(footprint=box(0, 0, 2, 1)), sampler, ())
        ground = np.array([[1.00000075, 0.4, 0.0]])

        self.assertEqual(
            sampler.sample(ground[:, :2]).reasons, ("ambiguous_projected_contact",)
        )
        np.testing.assert_allclose(passage.heights(ground), 0.02 * ground[:, 0])
        with self.assertRaisesRegex(ValueError, "complete floor"):
            passage.heights(np.array([[1.5, -0.2, 0.0]]))

    def test_dry_footway_and_raised_road_have_separate_usable_heights(self):
        footprint = box(-20, -5, 20, 5)
        span = WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1.25, 0)
        crossing = WaterCrossing(footprint, span, box(-20, -3, 20, 3), 1)
        source = SimpleNamespace(
            height=lambda xy: np.where(
                abs(xy[:, 0]) < 16,
                -2,
                0.002 * xy[:, 1]
                + 0.035 * xy[:, 0]
                + 0.8 * np.exp(-(((xy[:, 1] - 57.4) / 6) ** 2)),
            ),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000.0, 111000.0]),
            features=[
                ("water", box(-16, -150, 16, 150), {}),
                (
                    "transportation",
                    LineString([(-20, 0), (20, 0)]),
                    {"class": "secondary", "brunnel": "bridge", "layer": 1},
                ),
                (
                    "transportation",
                    LineString([(-120, 0), (-20, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(20, 0), (120, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(18, -100), (18, 100)]),
                    {"class": "path", "subclass": "footway"},
                ),
                (
                    "transportation",
                    LineString([(17, -8), (19, -8)]),
                    {"class": "path", "brunnel": "bridge", "layer": 1},
                ),
            ],
        )
        mesh = assemble(source, 150, crossings=[crossing])
        for y in np.linspace(-4.99, 4.99, 21):
            xy = (18, y)
            foot = support_height(mesh["sidewalk"], xy).max()
            underside = support_height(mesh["bridge deck"], xy).min()
            self.assertGreaterEqual(underside - foot, CLEARANCE_M)
            self.assertGreaterEqual(foot, -2 + FLOOR_ABOVE_WATER_M - 1e-6)
        for y in (-2.5, 0, 2.5):
            contact = (17.1, y)
            bank = support_height(mesh["ground"], contact).max()
            path = support_height(mesh["sidewalk"], contact).max()
            self.assertAlmostEqual(bank, path, places=5)
        sidewalk = support_height(mesh["sidewalk"], (18, -8))
        self.assertGreater(len(sidewalk), 0)
        self.assertLessEqual(np.ptp(sidewalk), 1e-6)
        stations = np.linspace(-59.9, 59.9, 601)
        for x in (17.2, 18, 18.8):
            z = [support_height(mesh["sidewalk"], (x, y)).max() for y in stations]
            self.assertLessEqual(
                np.max(abs(np.diff(z)) / np.diff(stations)), MAXIMUM_GRADE + 1e-5
            )
        for y in (-60, 60):
            for x in np.linspace(17.12, 18.88, 21):
                heights = support_height(mesh["sidewalk"], (x, y))
                self.assertGreater(len(heights), 1)
                self.assertAlmostEqual(np.ptp(heights), 0, places=5)
        for x in (-20, 20):
            left = support_height(mesh["road"], (x - 0.02, 0)).max()
            right = support_height(mesh["road"], (x + 0.02, 0)).max()
            self.assertLess(abs(left - right), 0.005)
