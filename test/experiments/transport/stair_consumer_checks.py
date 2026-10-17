"""Whole-body projection, height translation and local post-generation rejection."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import HeightInterval, Mesh, Reservation
from stair_consumer_buildings import body_query, one
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery


def body():
    vertices = np.array([[0, 0, -0.2], [2, 0, 0], [0, 2, 0], [0, 0, 3.4]])
    return Mesh(vertices, np.array([[0, 2, 1], [0, 1, 3], [1, 2, 3], [0, 3, 2]]))


class ConsumerChecks(unittest.TestCase):
    def test_band_and_projection_cover_actual_recipe_and_keep_local_arrays(self):
        local = body()
        original = local.vertices.copy()
        world, footprint = body_query("source-part", local, 162.5)
        np.testing.assert_array_equal(local.vertices, original)
        np.testing.assert_array_equal(world.vertices, original + [0, 0, 162.5])
        self.assertEqual(footprint.heights, HeightInterval(162.3, 165.9))
        self.assertTrue(
            footprint.geometry.equals(shapely.Polygon([[0, 0], [2, 0], [0, 2]]))
        )

    def test_pure_height_touch_is_free_but_actual_roof_height_conflicts(self):
        _, footprint = body_query("source-part", body(), 162.5)
        for lo, hi, wanted in ((165.9, 167, False), (165.8, 167, True)):
            query = OccupancyQuery(
                (
                    Reservation(
                        "bridge", footprint.geometry, HeightInterval(lo, hi), "body"
                    ),
                )
            )
            self.assertEqual(not query.conflict(footprint)[0].is_empty, wanted)

    def test_generated_overhang_cannot_pass_only_a_source_footprint_check(self):
        row = {"id": "original", "polygon": shapely.box(0, 0, 0.1, 0.1)}
        query = OccupancyQuery(
            (
                Reservation(
                    "road", shapely.box(1, 0, 2, 1), HeightInterval(162, 167), "body"
                ),
            )
        )
        contact = {"world_frame": "world", "root_enu_m": [0, 0, 162.5]}
        with (
            patch("stair_consumer_buildings.root_contact", return_value=contact),
            patch("stair_consumer_buildings.generated", return_value=(body(), {})),
            self.assertRaisesRegex(OccupancyError, "still enters transport"),
        ):
            one(row, [], {}, query, None)


if __name__ == "__main__":
    unittest.main()
