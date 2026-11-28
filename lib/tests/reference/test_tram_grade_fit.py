import unittest

import numpy as np
import shapely
from outshine.terrain.lattice import lattice, terrain_height
from outshine.transport.tram_grades import fit_tram_grades


class TramGradeFitTests(unittest.TestCase):
    def crossing(self, crossfall):
        axis = np.linspace(-24, 24, 49)
        prior, quads = lattice(axis, axis, lambda xy: xy @ [0.10, crossfall])
        line = shapely.LineString(((-7, 0), (7, 0)))
        carriageway = ((shapely.LineString(((0, -24), (0, 24))), 4.0, 0.14),)
        building = shapely.box(1, 5, 7, 8)
        fitted = fit_tram_grades(prior, quads, (line,), carriageway, (building,))
        border = (abs(prior[:, :2]) == 24).any(axis=1)
        protected = shapely.intersects_xy(building, *prior[:, :2].T)
        np.testing.assert_array_equal(fitted[:, :2], prior[:, :2])
        np.testing.assert_array_equal(
            fitted[border | protected], prior[border | protected]
        )
        return prior, fitted

    def slopes(self, vertices):
        x = np.linspace(-6, 6, 97)
        rails = x[:, None, None] * [1, 0] + np.array([[0, -1.435 / 2], [0, 1.435 / 2]])
        z = terrain_height(vertices, rails.reshape(-1, 2)).reshape(-1, 2)
        grade = abs(np.diff(z, axis=0) / np.diff(x)[:, None]).max()
        crosslevel = abs(z[:, 0] - z[:, 1]).max()
        road = terrain_height(vertices, np.column_stack((np.zeros_like(x), x)))
        road_grade = abs(np.diff(road) / np.diff(x)).max()
        return np.array((grade, crosslevel, road_grade))

    def test_fit_joins_track_and_street_while_pinning_tile_edges_and_buildings(self):
        _, fitted = self.crossing(0.10)
        self.assertTrue(np.all(self.slopes(fitted) <= [0.06001, 0.07501, 0.14001]))

    def test_fixed_contact_with_a_steep_road_allows_partial_improvement(self):
        prior, fitted = self.crossing(0.12)
        grade, crosslevel, road = self.slopes(fitted)
        old_grade, old_crosslevel, _ = self.slopes(prior)
        self.assertLess(grade, old_grade)
        self.assertLess(crosslevel, old_crosslevel)
        self.assertLessEqual(road, 0.14001)

    def test_track_with_compatible_ground_keeps_the_original_surface(self):
        axis = np.arange(-4, 5, dtype=float)
        prior, quads = lattice(axis, axis, lambda xy: xy @ [0.03, 0.015])
        line = shapely.LineString(((-3, 0), (3, 0)))
        self.assertIs(fit_tram_grades(prior, quads, (line,), (), ()), prior)
