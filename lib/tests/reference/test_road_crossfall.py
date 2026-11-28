import unittest

import numpy as np
import shapely
from outshine.transport.crown_constraints import CrownConstraints
from outshine.transport.road_crossfall import (
    GROUND,
    METRES_PER_CELL,
    WORLD_METRES,
    MercatorTile,
    geodetic,
    grade_crossfall,
)
from outshine.transport.route_graph import (
    RouteAttributes,
    RouteGraph,
    RouteNode,
    RoutePath,
)
from outshine.transport.route_tiles import WORLD_CELLS
from outshine.world.crown_surface import CrownPatch, CrownSurface
from outshine.world.road_terrain import _reservations


class RoadCrossfallTest(unittest.TestCase):
    def setUp(self):
        self.tile = MercatorTile((16, 8318, 32768))
        self.shape = (129, 129)
        self.origin = np.array(
            (self.tile.west + self.tile.span, 32768.5 * self.tile.span)
        )

    def road(self, *points, kind="primary"):
        cells = np.rint((np.array(points) + self.origin) / METRES_PER_CELL).astype(int)
        return RoutePath(
            ((14, 0, 0), 0),
            RouteAttributes(kind, GROUND, (1, -1), frozenset(), (("class", kind),)),
            tuple(RouteNode((int(x) % WORLD_CELLS, int(y)), GROUND) for x, y in cells),
        )

    def height(self, xy):
        east, south = np.moveaxis(xy - self.origin, -1, 0)
        east = (east + WORLD_METRES / 2) % WORLD_METRES - WORLD_METRES / 2
        return 200 + 0.04 * east + 0.40 * south

    def ground(self, tile, graph, reserved=(), absent=()):
        xy, _ = tile.grid(self.shape)
        raw = self.height(xy).reshape(self.shape)
        _, latitude = geodetic(xy.reshape(*self.shape, 2)[:, 0])
        crown, _ = grade_crossfall(
            tile, latitude, raw, graph, self.height, reserved, absent
        )
        return xy.reshape(*self.shape, 2) - self.origin, raw, crown

    def test_shared_seam_keeps_crown_and_longitudinal_grade(self):
        graph = RouteGraph((self.road((-120, 0), (0, 0), (120, 0)),), (), (), (), ())
        xy, raw, crown = self.ground(self.tile, graph)
        east = MercatorTile((16, 8319, 32768))
        _, _, adjacent = self.ground(east, graph)
        np.testing.assert_allclose(crown[:, -1], adjacent[:, 0], atol=1e-8, rtol=0)
        self.assertGreater(abs(raw[63, -1] - raw[64, -1]), 1.5)
        self.assertLess(abs(crown[63, -1] - crown[64, -1]), 0.13)
        run = np.diff(xy[64, -12:, 0])
        np.testing.assert_allclose(np.diff(crown[64, -12:]) / run, 0.04, atol=1e-7)
        np.testing.assert_array_equal(crown[:40], raw[:40])

    def test_junctions_reserved_ground_and_unknown_neighbours_stay_fixed(self):
        main = self.road((-210, 0), (-65, 0), (150, 0))
        trail = self.road((-65, 0), (-65, 60), kind="path")
        graph = RouteGraph((trail, main), (), (), (), ())
        reserve = shapely.box(-125, -3, -120, 3)
        missing = shapely.box(-15, -40, -5, 40)
        objects = [
            shapely.transform(shape, lambda xy: xy + self.origin)
            for shape in (reserve, missing)
        ]
        xy, raw, crown = self.ground(self.tile, graph, [objects[0]], [objects[1]])
        for contact in (-122, -65, -7):
            mask = (np.abs(xy[..., 0] - contact) < 5) & (np.abs(xy[..., 1]) < 6)
            self.assertTrue(mask.any())
            np.testing.assert_array_equal(crown[mask], raw[mask])
        mask = (np.abs(xy[..., 0] + 170) < 5) & (np.abs(xy[..., 1]) < 6)
        self.assertGreater(np.max(np.abs(crown[mask] - raw[mask])), 1.0)

    def test_overlapping_ways_without_a_shared_osm_vertex_keep_the_crossing(self):
        main = self.road((-180, 0), (100, 0))
        other = self.road((-60, -60), (-60, 60))
        graph = RouteGraph((main, other), (), (), (), ())
        xy, raw, crown = self.ground(self.tile, graph)
        crossing = (np.abs(xy[..., 0] + 60) < 5) & (np.abs(xy[..., 1]) < 5)
        self.assertTrue(crossing.any())
        np.testing.assert_array_equal(crown[crossing], raw[crossing])

    def test_overlapping_building_parts_reserve_each_footprint(self):
        shell = shapely.box(-12, -9, 12, 9)
        tower = shapely.box(-3, -5, 3, 5)
        grouped = shapely.MultiPolygon((shell, tower))
        self.assertFalse(grouped.is_valid)
        parts = tuple(_reservations((("building", grouped, {}),)))
        self.assertEqual(parts, (shell, tower))
        self.assertTrue(shapely.union_all(parts).covers(shapely.Point(0, 0)))

    def test_sharp_bend_preserves_the_original_support_until_a_curve_is_fit(self):
        road = self.road((-190, 0), (-80, 0), (80, -85))
        graph = RouteGraph((road,), (), (), (), ())
        xy, raw, crown = self.ground(self.tile, graph)
        bend = (np.abs(xy[..., 0] + 80) < 15) & (np.abs(xy[..., 1]) < 15)
        np.testing.assert_array_equal(crown[bend], raw[bend])
        straight = (np.abs(xy[..., 0] + 133) < 3) & (np.abs(xy[..., 1] + 5) < 3)
        self.assertGreater(np.max(np.abs(crown[straight] - raw[straight])), 1.0)

    def test_road_crown_crosses_the_antimeridian(self):
        self.origin[0] = WORLD_METRES
        west = MercatorTile((16, 65535, 32768))
        east = MercatorTile((16, 0, 32768))
        graph = RouteGraph((self.road((-120, 0), (0, 0), (120, 0)),), (), (), (), ())
        _, _, left = self.ground(west, graph)
        _, _, right = self.ground(east, graph)
        np.testing.assert_allclose(left[:, -1], right[:, 0], atol=1e-8, rtol=0)
        self.assertLess(abs(left[63, -1] - left[64, -1]), 0.13)


class CrownSurfaceTest(unittest.TestCase):
    def test_opposite_tiles_fit_one_shared_height_and_preserve_both_slopes(self):
        zero = np.zeros((3, 3))
        left_target = zero.copy()
        right_target = zero.copy()
        left_target[1] = (0, 1, 1)
        right_target[1] = (1, 0.5, 0)
        left = CrownPatch(
            zero,
            left_target,
            CrownConstraints(
                np.array([[3, 4, 5]]),
                np.array([[0, -1, 1]]),
                np.array([-0.2]),
                np.array([0.2]),
            ),
        )
        right = CrownPatch(
            zero,
            right_target,
            CrownConstraints(
                np.array([[3, 4, 5]]),
                np.array([[-1, 0.5, 0]]),
                np.array([-0.2]),
                np.array([0.2]),
            ),
        )
        for west, east in (((2, 1, 1), (2, 2, 1)), ((2, 3, 1), (2, 0, 1))):
            with self.subTest(west=west):
                patches = {west: left, east: right}
                first = CrownSurface(4, 2, patches.get).fit(west, left)
                second = CrownSurface(4, 2, patches.get).fit(east, right)
                np.testing.assert_allclose(first[1], (0, 0.9, 0.7), atol=1e-10)
                np.testing.assert_allclose(second[1], (0.7, 0.5, 0), atol=1e-10)
                np.testing.assert_array_equal(first[:, -1], second[:, 0])
                self.assertFalse(
                    CrownSurface(4, 2, {west: left}.get).fit(west, left).any()
                )
