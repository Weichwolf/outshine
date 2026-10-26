"""XY distance is bounded demand, not space admission; actual planes stay lazy."""

import gc
import unittest
import weakref
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame
from stair_occupancy_candidate_distance import FlatDistance
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

    def test_packed_snapshots_are_readonly_and_survive_source_replacement(self):
        prepared = FlatDistance()
        first = prepared.resolve(self.catalog.sources, 2)
        self.assertIs(prepared.resolve(self.catalog.sources, 2), first)
        expected = first.bounds.copy()
        other = (replace(self.catalog.sources[0], always=True),)
        second = prepared.resolve(other, 2)
        self.assertIsNot(first, second)
        np.testing.assert_array_equal(first.bounds, expected)
        np.testing.assert_array_equal(first.numeric, np.arange(2))
        self.assertEqual(len(second.numeric), 0)
        for layout in (first, second):
            for value in (layout.bounds, layout.starts, layout.numeric):
                self.assertFalse(value.flags.writeable)

    def test_face_budget_is_checked_before_packing_and_again_on_reuse(self):
        prepared = FlatDistance()
        with (
            patch(
                "stair_occupancy_candidate_distance.np.empty",
                side_effect=AssertionError(
                    "over-budget metadata must not be allocated"
                ),
            ),
            self.assertRaisesRegex(OccupancyError, "metadata face budget"),
        ):
            prepared.resolve(self.catalog.sources, 1)
        first = prepared.resolve(self.catalog.sources, 2)
        with patch(
            "stair_occupancy_candidate_distance.np.empty",
            side_effect=AssertionError("unchanged metadata must not be repacked"),
        ):
            with self.assertRaisesRegex(OccupancyError, "metadata face budget"):
                prepared.resolve(self.catalog.sources, 1)
            self.assertIs(prepared.resolve(self.catalog.sources, 2), first)

    def test_query_catalog_replacement_releases_previous_numeric_backing(self):
        bounds = self.catalog.sources[0].bounds.copy()
        bounds.flags.writeable = False
        released = weakref.ref(bounds)
        source = replace(self.catalog.sources[0], bounds=bounds)
        catalog = CandidateVolumes(self.catalog.frame, (source,))
        query = CandidateQuery(catalog)
        self.assertEqual(query.nearest_distance(shapely.Point(1, 1))[0], 0)
        query.catalog = self.catalog
        del catalog, source, bounds
        gc.collect()
        self.assertIsNone(released())
        self.assertEqual(query.nearest_distance(shapely.Point(1, 1))[0], 0)

    def test_always_source_cannot_be_pruned_by_its_numeric_box(self):
        bounds = np.array([[1e308, 1e308, 0, 1e308, 1e308, 1]], float)
        bounds.flags.writeable = False
        source = VolumeSource(
            "logical",
            bounds,
            self.catalog.sources[0].materialize,
            always=True,
            xy_geometry=lambda _index: shapely.Point(-1e308, -1e308),
        )
        query = CandidateQuery(
            CandidateVolumes(self.catalog.frame, (*self.catalog.sources, source))
        )
        distance, work = query.nearest_distance(shapely.Point(-1e308, -1e308))
        self.assertEqual(distance, 0)
        self.assertEqual(work["geometry_candidates"], 1)
        self.assertEqual(work["volume_materializer_calls"], 0)
        query.catalog = CandidateVolumes(self.catalog.frame, (source,))
        distance, work = query.nearest_distance(shapely.Point(-1e308, -1e308))
        self.assertEqual((distance, work["face_tests"]), (0, 1))
        query.budget = CandidateBudget(face_tests=0)
        with self.assertRaises(OccupancyError):
            query.nearest_distance(shapely.Point(-1e308, -1e308))


if __name__ == "__main__":
    unittest.main()
