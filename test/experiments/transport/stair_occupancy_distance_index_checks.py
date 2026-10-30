"""Indexed demand preserves actual distance, budgets and caller ownership."""

import gc
import unittest
import weakref
from dataclasses import replace
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame
from stair_occupancy_candidates import (
    CandidateBudget,
    CandidateQuery,
    CandidateVolumes,
    VolumeSource,
)
from stair_occupancy_distance_index import BoxDistance
from stair_occupancy_geometry import OccupancyError


def catalog(shapes, *, always=False, materialize=False, bounds=None):
    if bounds is None:
        bounds = [[*g.bounds[:2], 0, *g.bounds[2:], 1] for g in shapes]
    bounds = np.asarray(bounds, float)
    bounds.flags.writeable = False

    def build(index):
        if not materialize:
            raise AssertionError("XY demand must not construct physical volumes")
        return [(SimpleNamespace(geometry=shapes[index]), (), 0)]

    source = VolumeSource(
        "index-test",
        bounds,
        build,
        always,
        None if materialize else lambda index: shapes[index],
    )
    return CandidateVolumes(LocalFrame("index-test"), (source,))


class IndexChecks(unittest.TestCase):
    def test_actual_distance_beats_nearest_box_seed_and_matches_full_scan(self):
        shapes = (
            shapely.Polygon([(0, 0), (100, 0), (0, 100)]),
            shapely.Point(90, 90),
            shapely.LineString([(70, 80), (80, 90)]),
        )
        query = CandidateQuery(catalog(shapes), distance_method="indexed")
        geometries = [
            shapely.Point(80, 80),
            shapely.Point(0, 0),
            shapely.box(-10, -10, 110, 110),
            shapely.Polygon(
                [(60, 60), (95, 60), (95, 65), (65, 65), (65, 95), (60, 95)]
            ),
        ]
        for geometry in geometries:
            actual, work = query.nearest_distance(geometry)
            self.assertEqual(actual, min(geometry.distance(s) for s in shapes))
            self.assertEqual(work["volume_materializer_calls"], 0)
            self.assertTrue(work["face_tests_are_upper_bound"])
            self.assertIsNone(work["tree_internal_visits"])

    def test_unknown_bounds_and_empty_materializations_are_not_clearance(self):
        bounds = [[100, 100, 0, 101, 101, 1], [200, 200, 0, 201, 201, 1]]
        source = catalog(
            (shapely.Point(0, 0), shapely.Point(1, 0)), always=True, bounds=bounds
        )
        calls = []

        def build(index):
            calls.append(index)
            return (
                []
                if index == 0
                else [(SimpleNamespace(geometry=shapely.Point(0, 0)), (), 0)]
            )

        source = CandidateVolumes(
            source.frame,
            (replace(source.sources[0], materialize=build, xy_geometry=None),),
        )
        query = CandidateQuery(source, distance_method="indexed")
        for iteration in range(2):
            value, work = query.nearest_distance(shapely.Point(3, 4))
            self.assertEqual(value, 5)
            self.assertEqual(work["preparation_candidates"], 2 if iteration == 0 else 0)
        self.assertEqual(calls, [0, 1])
        empty = CandidateVolumes(
            source.frame, (replace(source.sources[0], materialize=lambda _: []),)
        )
        with self.assertRaisesRegex(OccupancyError, "no verified"):
            CandidateQuery(empty, distance_method="indexed").nearest_distance(
                shapely.Point(0, 0)
            )

    def test_cold_and_warm_budgets_and_low_budget_flat_fallback(self):
        source = catalog((shapely.Point(0, 0), shapely.Point(20, 0)))
        distance = BoxDistance()
        for budget in (
            CandidateBudget(products=0),
            CandidateBudget(face_tests=1),
            CandidateBudget(candidates=0),
        ):
            with self.assertRaises(OccupancyError):
                distance.query(source, shapely.Point(0, 0), budget)
        self.assertIsNone(distance.layout)
        value, work = distance.query(
            source, shapely.Point(0, 0), CandidateBudget(face_tests=2, candidates=1)
        )
        self.assertEqual(value, 0)
        self.assertNotIn("index_queries", work)
        _value, cold = distance.query(source, shapely.Point(0, 0), CandidateBudget())
        self.assertEqual(cold["face_tests"], 6)
        self.assertEqual(cold["preparation_metadata_boxes"], 2)
        layout = distance.layout
        for array in (layout.bounds, layout.ids, layout.starts):
            self.assertFalse(array.flags.writeable)
        with patch(
            "stair_occupancy_distance_index.prepare",
            side_effect=AssertionError("must reuse"),
        ):
            _value, warm = distance.query(
                source, shapely.Point(0, 0), CandidateBudget(face_tests=4)
            )
            self.assertEqual(warm["face_tests"], 4)
            self.assertEqual(warm["preparation_metadata_boxes"], 0)
            self.assertIs(distance.layout, layout)
            with self.assertRaises(OccupancyError):
                distance.query(
                    source, shapely.Point(0, 0), CandidateBudget(face_tests=1)
                )

    def test_catalog_replacement_releases_index_and_fallback_backing(self):
        for budget in (CandidateBudget(), CandidateBudget(face_tests=1)):
            source = catalog((shapely.Point(0, 0),))
            reference = weakref.ref(source.sources[0].bounds)
            distance = BoxDistance()
            distance.query(source, shapely.Point(1, 1), budget)
            other = catalog((shapely.Point(10, 10),))
            distance.query(other, shapely.Point(10, 10), budget)
            del source
            gc.collect()
            self.assertIsNone(reference())

    def test_failed_preparation_is_not_published_and_can_retry(self):
        source = catalog((shapely.Point(0, 0),), always=True)
        distance = BoxDistance()
        with (
            patch(
                "stair_occupancy_distance_index.footprints",
                side_effect=OccupancyError("incomplete"),
            ),
            self.assertRaises(OccupancyError),
        ):
            distance.query(source, shapely.Point(1, 1), CandidateBudget())
        self.assertIsNone(distance.layout)
        self.assertEqual(
            distance.query(source, shapely.Point(0, 0), CandidateBudget())[0], 0
        )

    def test_invalid_queries_and_escaping_footprints_fail(self):
        source = catalog((shapely.Point(0, 0),))
        for geometry in (
            shapely.Polygon(),
            shapely.Polygon([(0, 0), (1, 1), (0, 1), (1, 0)]),
        ):
            with self.assertRaises(OccupancyError):
                BoxDistance().query(source, geometry, CandidateBudget())
        invalid = catalog((shapely.Point(2, 2),), bounds=[[0, 0, 0, 1, 1, 1]])
        with self.assertRaisesRegex(OccupancyError, "escapes"):
            BoxDistance().query(invalid, shapely.Point(0, 0), CandidateBudget())

    def test_outside_local_index_range_falls_back_without_extra_budget(self):
        for bounds in (None, [[0, 0, 0, 1, 1, 1]]):
            source = catalog((shapely.Point(2e6, 2e6),), always=True, bounds=bounds)
            value, work = BoxDistance().query(
                source, shapely.Point(2e6, 2e6), CandidateBudget()
            )
            self.assertEqual(value, 0)
            self.assertNotIn("index_queries", work)
            self.assertLessEqual(
                work["geometry_candidates"] + work.get("preparation_candidates", 0),
                8192,
            )

    def test_selected_candidate_exhaustion_does_not_return_seed_distance(self):
        source = catalog(
            (
                shapely.Polygon([(0, 0), (100, 0), (0, 100)]),
                shapely.Point(101, 90),
            )
        )
        distance = BoxDistance()
        with self.assertRaisesRegex(OccupancyError, "candidate budget"):
            distance.query(source, shapely.Point(90, 90), CandidateBudget(candidates=1))
        self.assertEqual(
            distance.query(source, shapely.Point(90, 90), CandidateBudget())[0], 11
        )

    def test_generated_local_geometries_equal_independent_full_footprint_scan(self):
        rng = np.random.default_rng(1785)
        shapes = tuple(
            shapely.Polygon(p).convex_hull for p in rng.uniform(-500, 500, (50, 3, 2))
        )
        query = CandidateQuery(catalog(shapes), distance_method="indexed")
        for xy in rng.uniform(-700, 700, (100, 2)):
            for geometry in (shapely.Point(xy), shapely.box(*xy, *(xy + [20, 30]))):
                value, _work = query.nearest_distance(geometry)
                self.assertEqual(value, min(geometry.distance(s) for s in shapes))


if __name__ == "__main__":
    unittest.main()
