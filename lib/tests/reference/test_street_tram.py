import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.terrain.ground_surface import terrain_triangles
from outshine.terrain.lattice import lattice
from outshine.transport.tram_rails import (
    FLANGEWAY_WIDTH_M,
    street_tram_surfaces,
)


class StreetTramTests(unittest.TestCase):
    def setUp(self):
        self.extent = shapely.box(-12, -8, 12, 8)
        self.road = shapely.box(-2, -8, 2, 8)
        self.line = shapely.LineString(((-10, 0), (10, 0)))
        self.vertices, quads = lattice((-12, 0, 12), (-8, 0, 8), np.zeros((3, 3)))
        self.faces = terrain_triangles(quads)

    def surfaces(self, *decks):
        return street_tram_surfaces(
            (self.line,), self.extent, self.road, (), decks, self.vertices, self.faces
        )

    def test_tracks_cross_asphalt_and_leave_clear_flange_spaces(self):
        surfaces = self.surfaces()
        rails = surfaces["tram rail"]
        flanges = surfaces["tram flangeway"]
        slab = surfaces["tram slab"]
        cross = shapely.LineString(((0, -2), (0, 2)))
        rail_positions = sorted(
            part.bounds[1::2] for part in shapely.get_parts(rails.intersection(cross))
        )
        self.assertEqual(len(rail_positions), 2)
        self.assertAlmostEqual(rail_positions[1][0] - rail_positions[0][1], 1.435)
        self.assertAlmostEqual(
            flanges.intersection(cross).length, 2 * FLANGEWAY_WIDTH_M
        )
        self.assertAlmostEqual(rails.intersection(flanges).area, 0)
        self.assertTrue(rails.covers(shapely.Point(5, 0.7535)))
        self.assertTrue(slab.covers(shapely.Point(5, 0)))
        self.assertAlmostEqual(slab.intersection(self.road).area, 0)

    def test_tram_uses_lower_viaduct_space_only_with_clearance(self):
        footprint = shapely.box(-2, -5, 2, 5)
        routes = shapely.box(-3, -1.6, 3, 1.6)
        for height, supported in ((6.55, True), (6.4, False)):
            deck = SimpleNamespace(
                footprint=footprint,
                lower_routes=routes,
                height=lambda points, z=height: z,
            )
            rails = self.surfaces(deck)["tram rail"]
            self.assertEqual(rails.covers(shapely.Point(0, 0.7535)), supported)
            self.assertTrue(rails.covers(shapely.Point(6, 0.7535)))
