import unittest
from functools import partial
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.terrain.ground_surface import ROAD_WIDTH
from outshine.terrain.lattice import boundary_stations, lattice, terrain_height
from outshine.transport.heights import _face_gradients
from outshine.transport.road_grades import _reserved_cells, fit_road_grades
from outshine.world.mesh import assemble, terrain_grid
from outshine.world.road_terrain import (
    RoadTerrain,
    local_metric,
    metres_at,
    terrain_tiles,
)
from outshine.world.scene import support_height


class RoadGradeTest(unittest.TestCase):
    def test_regional_seam_can_reconstruct_the_clipped_terrain_planes(self):
        vertices, _ = lattice(
            [-2.0, -0.2, 1.4, 2.4],
            [-3.0, -1.0, 0.6, 2.6],
            lambda xy: xy[:, 0] * xy[:, 1] + xy[:, 1] ** 2,
        )
        samples = np.linspace(-1.0, 1.0, 51)
        east, north = boundary_stations(vertices, 1.0)
        for edge in (-1.0, 1.0):
            for axis, stations in ((0, north), (1, east)):

                def points(positions, fixed=axis, seam=edge):
                    pair = np.full((len(positions), 2), seam)
                    pair[:, 1 - fixed] = positions
                    return pair

                reference = terrain_height(vertices, points(samples))
                fitted = np.interp(
                    samples, stations, terrain_height(vertices, points(stations))
                )
                np.testing.assert_allclose(fitted, reference, rtol=0, atol=1e-9)

    def test_reservation_cells_include_small_polygons_holes_and_linear_contacts(self):
        source = SimpleNamespace(height=lambda p: np.zeros(len(p)))
        vertices, quads = terrain_grid(source, 3)
        polygon = shapely.box(-2.5, -1.8, 2, 1.7).difference(shapely.box(-1, -1, 1, 1))
        tiny = shapely.box(0.12, 0.12, 0.13, 0.13)
        line = shapely.LineString([(2.1, -1.4), (2.9, 1.9)])
        cells = shapely.polygons(vertices[quads, :2])
        expected = shapely.intersects(shapely.union_all([polygon, tiny, line]), cells)
        np.testing.assert_array_equal(
            _reserved_cells(vertices, quads, (polygon, tiny, line)), expected
        )

    def test_rendered_road_and_shoulders_meet_at_the_fitted_height(self):
        tags = {"class": "secondary"}
        lane = shapely.LineString([(-20, 0), (20, 0)])
        source = SimpleNamespace(
            height=lambda p: 2.0 * np.maximum(1.0 - (p[:, 0] / 7) ** 2, 0.0),
            features=[("transportation", lane, tags)],
        )
        vertices, quads = terrain_grid(source, 24)
        result = fit_road_grades(
            vertices,
            quads,
            {0.12: lane.buffer(ROAD_WIDTH["secondary"] / 2)},
            shapely.Polygon(),
        )
        source.road_terrain = SimpleNamespace(grid=lambda _: (result.vertices, quads))
        product = assemble(source, 16)
        self.assertLess(support_height(product["road"], (0, 0)).max(), 1.5)
        self.assertLessEqual(
            np.max(abs(np.asarray(product["ground"][0])[:, :2])), 16.000001
        )
        wider = assemble(source, 19)
        for x in (-5, 0, 5):
            for edge in (-ROAD_WIDTH["secondary"] / 2, ROAD_WIDTH["secondary"] / 2):
                road = support_height(product["road"], (x, edge))
                soil = support_height(product["ground"], (x, edge))
                self.assertAlmostEqual(road.max(), soil.max(), places=7)
                self.assertAlmostEqual(
                    road.max(), support_height(wider["road"], (x, edge)).max(), places=7
                )

    def test_world_lattice_has_the_same_support_in_shifted_views(self):
        geo = np.array([[0.0009, 0.0012], [-0.0003, 0.0006], [0.0012, -0.0002]])
        world_height = lambda lat, lon: 300 + 2 * np.sin(lat * 500) + np.cos(lon * 600)
        observations = []
        for lon, lat, radius in (
            (0, 0, 155),
            (0.0002, 0.0003, 201),
            (0.001, 0.0005, 160),
        ):
            metres = metres_at(lat)
            project = partial(local_metric, origin=(lon, lat), metres=metres)
            keys = terrain_tiles(lat, lon, radius)
            source = SimpleNamespace(
                lat=lat,
                lon=lon,
                metres=metres,
                metric=project,
                datum=world_height(lat, lon),
                metric_features=lambda *args, **kwargs: (),
                dem=SimpleNamespace(
                    sample_with_zoom=lambda y, x, **options: (world_height(y, x), None)
                ),
            )
            terrain = RoadTerrain(
                source, {(14, x >> 2, y >> 2): {} for _, x, y in keys}
            )
            vertices, _ = terrain.grid(radius)
            observations.append(
                terrain_height(vertices, project(*geo.T)) + source.datum
            )
        for observed in observations[1:]:
            np.testing.assert_allclose(observed, observations[0], rtol=0, atol=1e-9)

    def test_world_lattice_joins_the_antimeridian(self):
        lon, lat = 179.9997, 0
        scale = metres_at(lat)
        project = partial(local_metric, origin=(lon, lat), metres=scale)

        def elevation(y, x, **options):
            self.assertTrue((np.abs(x) < 180.0).all())
            return 300 + 2 * np.sin(np.deg2rad(x)), None

        source = SimpleNamespace(
            lat=lat,
            lon=lon,
            metres=scale,
            metric=project,
            datum=300,
            metric_features=lambda *args, **kwargs: (),
            dem=SimpleNamespace(sample_with_zoom=elevation),
        )
        keys = terrain_tiles(lat, lon, 80)
        terrain = RoadTerrain(source, {(14, x >> 2, y >> 2): {} for _, x, y in keys})
        vertices, _ = terrain.grid(80)
        xy = project(np.array([179.9998, 180.0, -179.9998]), np.zeros(3))
        expected = 2 * np.sin(np.deg2rad([179.9998, 180.0, -179.9998]))
        np.testing.assert_allclose(terrain_height(vertices, xy), expected, atol=1e-9)

    def test_steep_connected_pavement_uses_one_surface_with_fixed_terrain_edges(self):
        terrain = SimpleNamespace(
            height=lambda p: 2.0 * np.maximum(1.0 - (p[:, 0] / 7) ** 2, 0.0)
        )
        vertices, quads = terrain_grid(terrain, 24)
        road = shapely.box(-12, -2.5, 12, 2.5).union(shapely.box(-2.5, -8, 2.5, 8))
        result = fit_road_grades(vertices, quads, {0.12: road}, shapely.Polygon())
        self.assertGreater(np.max(abs(result.vertices[:, 2] - vertices[:, 2])), 0.5)
        self.assertGreater(len(result.faces), 0)
        self.assertEqual(result.unresolved_faces, 0)
        xy = vertices[:, :2]
        boundary = (abs(xy) == 24).any(axis=1)
        np.testing.assert_array_equal(result.vertices[boundary], vertices[boundary])
        np.testing.assert_array_equal(result.vertices[:, :2], xy)
        grades = _face_gradients(xy, result.faces, result.vertices[:, 2])
        self.assertTrue(np.all(np.linalg.norm(grades, axis=1) <= result.limits + 1e-7))

    def test_existing_structure_contacts_keep_their_elevation(self):
        terrain = SimpleNamespace(height=lambda p: p[:, 0] * 0.32)
        vertices, quads = terrain_grid(terrain, 24)
        road = shapely.box(-20, -3, 20, 3)
        result = fit_road_grades(vertices, quads, {0.1: road}, road.buffer(2))
        np.testing.assert_array_equal(result.vertices, vertices)
        self.assertEqual(len(result.faces), 0)
        self.assertGreater(result.unresolved_faces, 0)
