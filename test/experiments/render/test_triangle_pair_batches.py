"""Bounded streaming, cross-chunk ties, real scalar controls and unknown math."""

import unittest

import numpy as np
from test_triangle_columns import CAPTURE, POINTS, WORLD, rectangles
from triangle_columns import nearest, prepare, query
from triangle_pair_batches import MAX_PAIRS, TriangleBatches


class TriangleBatchTests(unittest.TestCase):
    def test_whole_packets_and_oversized_rays_preserve_distance_then_ordinal(self):
        source = rectangles()
        triangles = np.tile(source["indices"], (67, 1))
        origins = np.array([[0, 0, 4], [0.1, -0.1, 4], [2, 2, 4]], float)
        directions = np.tile([0.0, 0.0, -1.0], (3, 1))
        candidates = [
            np.arange(len(triangles)),
            np.arange(3, len(triangles)),
            np.arange(len(triangles)),
        ]
        expected = [
            nearest(source["positions"], triangles, faces, o, d)
            for faces, o, d in zip(candidates, origins, directions, strict=True)
        ]
        for capacity in (1, 3, 17, 512):
            batch = TriangleBatches(
                source["positions"], triangles, origins, directions, capacity
            )
            for row, faces in enumerate(candidates):
                batch.add(row, faces)
            batch.flush()
            np.testing.assert_array_equal(batch.distance, [v[0] for v in expected])
            np.testing.assert_array_equal(batch.owner, [v[1] for v in expected])
            self.assertFalse(batch.unknown.any())
            self.assertEqual(batch.tested, sum(map(len, candidates)))
            self.assertEqual(
                batch.single_ray_calls, sum(len(c) > capacity for c in candidates)
            )
            self.assertLessEqual(batch.peak_pairs, capacity)
            self.assertEqual(
                batch.work()["intersection_staging_payload_bytes"], capacity * 12
            )
            with self.assertRaises(ValueError):
                batch.add(2, candidates[2])

    def test_full_query_and_silhouette_neighbours_match_scalar_for_chunk_sizes(self):
        source = rectangles()
        field = prepare(source, CAPTURE, 16)
        clip = np.c_[source["positions"], np.ones(len(source["positions"]))] @ WORLD.T
        edges = clip[:, :2] / clip[:, 3:] * [1, -1] + 1
        rng = np.random.default_rng(47)
        points = np.r_[
            POINTS,
            edges,
            np.nextafter(edges, -np.inf),
            np.nextafter(edges, np.inf),
            rng.uniform(-2, 4, (80, 2)),
        ]
        expected = query(field, WORLD, points, [2, 2], pair_batch_size=0)
        for capacity in (1, 2, 3, 7, 512, 4096, MAX_PAIRS):
            result = query(field, WORLD, points, [2, 2], pair_batch_size=capacity)
            for actual, control in zip(result[:3], expected[:3], strict=True):
                np.testing.assert_array_equal(actual, control)
            self.assertEqual(result[3]["triangle_tests"], expected[3]["triangle_tests"])
            self.assertLessEqual(
                result[3]["intersection_work"]["max_intersection_batch_pairs"], capacity
            )

    def test_reference_exhaustion_flushes_prior_rows_and_keeps_remainder_unknown(self):
        field = prepare(rectangles(), CAPTURE, 16)
        first = query(field, WORLD, POINTS[:1], [2, 2], pair_batch_size=0)
        budget = first[3]["triangle_references"]
        expected = query(
            field, WORLD, POINTS, [2, 2], pair_batch_size=0, pairs_most=budget
        )
        self.assertTrue(expected[2][0])
        self.assertFalse(expected[2][1:].all())
        for capacity in (1, 3, MAX_PAIRS):
            result = query(
                field,
                WORLD,
                POINTS,
                [2, 2],
                pair_batch_size=capacity,
                pairs_most=budget,
            )
            for actual, control in zip(result[:3], expected[:3], strict=True):
                np.testing.assert_array_equal(actual, control)

    def test_empty_work_allocates_no_batch_and_invalid_capacities_are_refused(self):
        field = prepare(rectangles(), CAPTURE, 16)
        result = query(field, WORLD, np.array([[-100, -100]]), [2, 2])
        self.assertTrue(result[2].all())
        self.assertIsNone(result[3]["intersection_work"])
        for capacity in (-1, MAX_PAIRS + 1, True, 1.5):
            with self.assertRaises(ValueError):
                query(field, WORLD, POINTS, [2, 2], pair_batch_size=capacity)

    def test_nonfinite_candidate_arithmetic_is_unknown_instead_of_empty(self):
        positions = np.array(
            [[-1e30, -1e30, 1e30], [1e30, -1e30, 1e30], [0, 1e30, 1e30]], np.float32
        )
        batch = TriangleBatches(
            positions,
            np.array([[0, 1, 2]]),
            np.array([[0, 0, 1e308]]),
            np.array([[0, 0, -1.0]]),
            1,
        )
        batch.add(0, np.array([0], np.uint32))
        batch.flush()
        self.assertTrue(batch.unknown[0])
        self.assertEqual(batch.work()["intersection_numeric_unknown_samples"], 1)
        self.assertEqual(batch.owner[0], -1)


if __name__ == "__main__":
    unittest.main()
