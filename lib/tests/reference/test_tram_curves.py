import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.transport.rail_routes import surface_rail_lines
from outshine.transport.route_graph import route_graph
from outshine.transport.route_tiles import WORLD_CELLS, geography
from outshine.world.road_terrain import RoadTerrain, local_metric, metres_at

TILE = (14, 8192, 8191)
EXTENT = 4096
TRAM = {"class": "transit", "subclass": "tram"}
COURSE = [(970, 1000), (1020, 1000), (1050, 990), (1080, 970)]


def way(identifier, points, tags=TRAM):
    return {
        "id": identifier,
        "properties": tags,
        "geometry": {"type": "LineString", "coordinates": points},
    }


def project(longitude, latitude):
    return local_metric(longitude, latitude, (0, 0), metres_at(0))


def peak_turn(line):
    vectors = np.diff(np.asarray(line.coords), axis=0)
    angles = np.arctan2(vectors[:, 1], vectors[:, 0])
    return np.max(abs(np.diff(np.unwrap(angles))))


class TramCurveTest(unittest.TestCase):
    def axes(self, *features, bounds=None):
        source = SimpleNamespace(metric_features=lambda *args, **kwargs: ())
        tile = {TILE: {"extent": EXTENT, "features": [way(1, COURSE), *features]}}
        terrain = RoadTerrain(source, {TILE: {"transportation": tile[TILE]}})
        graph = route_graph(tile, bounds=bounds)
        raw = tuple(surface_rail_lines(graph, project, kind="transit", mode="tram"))
        return graph, raw, terrain.tram_lines(graph, project)

    def test_same_rounded_geometry_for_a_tile_and_a_smaller_crop(self):
        _, (raw,), (full,) = self.axes()
        stride = WORLD_CELLS >> TILE[0]
        cell = stride // EXTENT
        x, y = TILE[1] * stride, TILE[2] * stride
        _, _, (cropped,) = self.axes(
            bounds=(x + 1015 * cell, y + 985 * cell, x + 1045 * cell, y + 1005 * cell)
        )
        np.testing.assert_array_equal(full.coords, cropped.coords)
        np.testing.assert_array_equal(full.coords[0], raw.coords[0])
        np.testing.assert_array_equal(full.coords[-1], raw.coords[-1])
        self.assertLess(full.hausdorff_distance(raw), 0.45)
        self.assertLess(peak_turn(full), peak_turn(raw) / 4)

    def test_preserves_a_mapped_road_contact_and_the_diamond_approach(self):
        street = way(2, [(1020, 960), COURSE[1], (1020, 1040)], {"class": "secondary"})
        graph, (raw,), (curved,) = self.axes(street)
        [contact] = graph.contacts
        np.testing.assert_array_equal(curved.coords[1], raw.coords[1])
        point = shapely.Point(project(*geography(contact.rail.point))[0])
        self.assertTrue(curved.covers(point))
        self.assertLess(len(raw.coords), len(curved.coords))

        diamond = way(3, [(1020, 960), (1020, 1040)])
        _, inputs, outputs = self.axes(diamond)
        original_contact = inputs[0].intersection(inputs[1])
        self.assertTrue(original_contact.equals(outputs[0].intersection(outputs[1])))

    def test_rounds_parallel_tracks_without_consuming_the_bed_clearance(self):
        for offset in (4, 5):
            with self.subTest(offset=offset):
                other = way(2, [(x, y + offset) for x, y in COURSE])
                _, before, after = self.axes(other)
                self.assertTrue(all(len(line.coords) > len(COURSE) for line in after))
                old_gap = before[0].distance(before[1])
                self.assertGreaterEqual(
                    after[0].distance(after[1]), min(old_gap, 2.04) - 1e-7
                )
