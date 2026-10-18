"""Local work limits never turn an unresolved whole-footprint query into free space."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval, LocalFrame
from stair_occupancy_candidate_reuse import retained
from stair_occupancy_candidate_sources import numeric_bounds
from stair_occupancy_candidates import (
    CandidateBudget,
    CandidateQuery,
    CandidateVolumes,
    VolumeSource,
)
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import volume
from stair_shared_headroom_volumes import upper_band


class BoundedVolumeCandidates(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.frame = LocalFrame("candidate-controls")
        lower = np.array([[[0, 0, 0], [4, 0, 0], [0, 4, 0]]], float)
        upper = lower + [0, 0, 1]
        self.rows, sources = [], []
        for name, delta in (("near", 0), ("far", 100)):
            lo, hi = lower + [delta, 0, 0], upper + [delta, 0, 0]
            row, error = volume(name, lo[0], hi[0], 2.5)
            self.rows.append(row)

            def make(index, name=name, row=row, error=error):
                self.calls.append(name)
                return ((row, (name,), error),)

            sources.append(VolumeSource(name, numeric_bounds(lo, hi, 2.5), make))
        logical = upper_band(self.rows[0], self.rows[0].geometry, 2, "logical")

        def make_logical(index):
            self.calls.append("logical")
            return ((logical, ("car", "walk"), 0),)

        self.rows.append(logical)
        sources.append(
            VolumeSource(
                "logical", numeric_bounds(lower, upper, 4.5), make_logical, always=True
            )
        )
        self.sources = tuple(sources)
        self.catalog = CandidateVolumes(self.frame, self.sources)
        self.footprint = BuildingFootprint(
            "building", shapely.box(0.1, 0.1, 0.5, 0.5), HeightInterval(4, 4.5)
        )

    def test_logical_headroom_survives_height_and_product_boundaries(self):
        expected, _ = OccupancyQuery(tuple(self.rows)).conflict(self.footprint)
        self.assertFalse(expected.is_empty)
        for method in ("flat", "coarse"):
            actual, _ = CandidateQuery(self.catalog, method=method).conflict(
                self.footprint
            )
            self.assertTrue(actual.equals(expected))
        self.assertNotIn("far", self.calls)

    def test_each_exhausted_budget_is_incomplete_before_any_plane_build(self):
        for budget in (
            CandidateBudget(products=0),
            CandidateBudget(face_tests=0),
            CandidateBudget(candidates=0),
        ):
            with self.subTest(budget=budget):
                result = self.catalog.query(self.frame, self.footprint, budget=budget)
                self.assertFalse(result.complete)
                self.assertEqual(result.volumes, ())
                with self.assertRaises(OccupancyError):
                    CandidateQuery(self.catalog, budget=budget).conflict(self.footprint)
                self.assertEqual(self.calls, [])

    def test_duplicate_product_reference_does_not_duplicate_work_or_ids(self):
        repeated = CandidateVolumes(self.frame, self.sources + self.sources)
        a = self.catalog.query(self.frame, self.footprint)
        b = repeated.query(self.frame, self.footprint)
        self.assertEqual(a.volumes, b.volumes)
        self.assertEqual(a.source_ids, b.source_ids)
        self.assertEqual(a.work, b.work)

    def test_retained_facets_are_bounded_identical_and_reused(self):
        catalog, info = retained(self.catalog, 2)
        first = catalog.query(self.frame, self.footprint)
        calls = len(self.calls)
        second = catalog.query(self.frame, self.footprint)
        self.assertEqual(first.volumes, second.volumes)
        self.assertEqual(first.source_ids, second.source_ids)
        self.assertEqual(len(self.calls), calls)
        self.assertGreater(info().hits, 0)
        self.assertLessEqual(info().currsize, 2)
        catalog, info = retained(self.catalog, 1)
        both = BuildingFootprint(
            "both-bands", self.footprint.geometry, HeightInterval(0.1, 4.5)
        )
        catalog.query(self.frame, both)
        catalog.query(self.frame, both)
        self.assertEqual(info().currsize, 1)
        self.assertEqual(info().hits, 0)
        self.assertGreater(info().misses, 1)

    def test_retention_never_bypasses_selection_or_capacity(self):
        catalog, info = retained(self.catalog, 2)
        catalog.query(self.frame, self.footprint)
        calls = len(self.calls)
        result = catalog.query(
            self.frame, self.footprint, budget=CandidateBudget(candidates=0)
        )
        self.assertFalse(result.complete)
        self.assertEqual(len(self.calls), calls)
        self.assertEqual(info().hits, 0)
        for maximum in (0, 8193, True):
            with self.assertRaises(ValueError):
                retained(self.catalog, maximum)

    def test_escaped_metadata_does_not_expose_free_space(self):
        source = self.sources[0]
        bounds = source.bounds.copy()
        bounds[:, 5] = 0.5
        bounds.setflags(write=False)
        catalog = CandidateVolumes(
            self.frame, (VolumeSource(source.key, bounds, source.materialize),)
        )
        footprint = BuildingFootprint(
            "cap-error", self.footprint.geometry, HeightInterval(0.1, 0.2)
        )
        result = catalog.query(self.frame, footprint)
        self.assertFalse(result.complete)
        self.assertIn("escapes numeric metadata", result.reason)
        with self.assertRaises(OccupancyError):
            CandidateQuery(catalog).conflict(footprint)

    def test_wrong_frame_and_ambiguous_duplicate_product_are_rejected(self):
        with self.assertRaises(ValueError):
            self.catalog.query(LocalFrame("other-frame"), self.footprint)
        source = self.sources[0]
        ambiguous = VolumeSource(source.key, source.bounds, source.materialize)
        with self.assertRaises(ValueError):
            CandidateVolumes(self.frame, self.sources + (ambiguous,))


if __name__ == "__main__":
    unittest.main()
