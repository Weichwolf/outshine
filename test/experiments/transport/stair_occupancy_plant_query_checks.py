"""Every actual building part survives, including equal owners on separate bodies."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import (
    BuildingFootprint,
    HeightInterval,
    LocalFrame,
    Reservation,
)
from stair_occupancy_candidate_sources import numeric_bounds
from stair_occupancy_candidates import CandidateQuery, CandidateVolumes, VolumeSource
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_inputs import volume
from stair_occupancy_plant_query import PlantQuery


class WholePlantQuery(unittest.TestCase):
    def setUp(self):
        low = np.array([[[0, 0, 0], [2, 0, 0], [0, 2, 0]]], float)
        high = low + [0, 0, 1]
        row, error = volume("road", low[0], high[0], 1)
        source = VolumeSource(
            "road",
            numeric_bounds(low, high, 1),
            lambda _: ((row, ("road-source",), error),),
        )
        catalog = CandidateVolumes(LocalFrame("whole-plants-control"), (source,))
        buildings = tuple(
            Reservation(
                "same-source-part",
                shapely.box(x, 0, x + 1, 1),
                HeightInterval(0, 3),
                "building.body",
            )
            for x in (5, 10)
        )
        self.query = PlantQuery(CandidateQuery(catalog), buildings)

    def test_both_equal_owner_body_parts_block_their_actual_space(self):
        for x in (5, 10):
            footprint = BuildingFootprint(
                "plant", shapely.box(x + 0.2, 0.2, x + 0.8, 0.8), HeightInterval(0.1, 1)
            )
            conflict, _cost = self.query.conflict(footprint)
            self.assertTrue(conflict.equals(footprint.geometry))
        self.assertEqual(len(self.query.buildings.reservations), 2)

    def test_xy_distance_does_not_certify_an_overhead_band(self):
        geometry = shapely.box(5.2, 0.2, 5.8, 0.8)
        distance, _cost = self.query.nearest_distance(geometry)
        self.assertEqual(distance, 0)
        footprint = BuildingFootprint("overhead", geometry, HeightInterval(4, 5))
        conflict, _cost = self.query.conflict(footprint)
        self.assertTrue(conflict.is_empty)

    def test_incomplete_transport_is_not_masked_by_a_clear_building_query(self):
        class Incomplete:
            def conflict(self, footprint, **kwargs):
                raise OccupancyError("candidate budget exhausted")

        query = PlantQuery(Incomplete(), self.query.buildings.reservations)
        with self.assertRaises(OccupancyError):
            query.conflict(
                BuildingFootprint(
                    "unresolved", shapely.box(30, 0, 31, 1), HeightInterval(0, 1)
                )
            )


if __name__ == "__main__":
    unittest.main()
