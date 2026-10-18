"""XY distance is bounded demand, not space admission; actual planes stay lazy."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame
from stair_occupancy_candidate_sources import numeric_bounds, xy_geometry
from stair_occupancy_candidates import (
    CandidateBudget,
    CandidateQuery,
    CandidateVolumes,
    VolumeSource,
)
from stair_occupancy_geometry import OccupancyError


class DistanceChecks(unittest.TestCase):
    def setUp(self):
        triangles = np.array(
            [[[0, 0, 0], [4, 0, 0], [0, 4, 0]], [[20, 0, 0], [24, 0, 0], [20, 4, 0]]],
            float,
        )
        self.calls = []

        def materialize(index):
            self.calls.append(index)
            raise AssertionError("physical planes must not be built for XY order")

        source = VolumeSource(
            "physical",
            numeric_bounds(triangles, triangles + [0, 0, 1], 0),
            materialize,
            xy_geometry=xy_geometry(
                triangles.reshape(-1, 3), np.arange(6).reshape(2, 3)
            ),
        )
        self.catalog = CandidateVolumes(LocalFrame("distance-test"), (source,))
        self.triangles = [shapely.Polygon(t[:, :2]) for t in triangles]

    def test_points_and_whole_polygons_match_full_xy_distance_without_planes(self):
        queries = [
            shapely.Point(1, 1),
            shapely.Point(3, 3),
            shapely.Point(-2, -3),
            shapely.Point(12, 2),
            shapely.box(19, -1, 21, 1),
            shapely.box(-100, -100, 100, 100),
        ]
        for geometry in queries:
            expected = min(geometry.distance(t) for t in self.triangles)
            actual, work = CandidateQuery(self.catalog).nearest_distance(geometry)
            self.assertEqual(actual, expected)
            self.assertLessEqual(work["geometry_candidates"], 2)
            self.assertEqual(work["volume_materializer_calls"], 0)
        self.assertEqual(self.calls, [])

    def test_budgets_fail_explicitly_instead_of_returning_infinite_clearance(self):
        for budget in (
            CandidateBudget(products=0),
            CandidateBudget(face_tests=0),
            CandidateBudget(candidates=0),
        ):
            with self.assertRaises(OccupancyError):
                CandidateQuery(self.catalog, budget=budget).nearest_distance(
                    shapely.Point(3, 3)
                )
        self.assertEqual(self.calls, [])

    def test_zero_distance_finishes_after_one_selected_shape(self):
        distance, work = CandidateQuery(
            self.catalog, budget=CandidateBudget(candidates=1)
        ).nearest_distance(shapely.box(-100, -100, 100, 100))
        self.assertEqual(distance, 0)
        self.assertEqual(work["geometry_candidates"], 1)

    def test_invalid_and_empty_sources_are_not_free(self):
        for geometry in (
            shapely.Polygon(),
            shapely.Polygon([(0, 0), (1, 1), (0, 1), (1, 0)]),
        ):
            with self.assertRaises(OccupancyError):
                CandidateQuery(self.catalog).nearest_distance(geometry)
        empty = CandidateVolumes(self.catalog.frame, ())
        with self.assertRaises(OccupancyError):
            CandidateQuery(empty).nearest_distance(shapely.Point(0, 0))


if __name__ == "__main__":
    unittest.main()
