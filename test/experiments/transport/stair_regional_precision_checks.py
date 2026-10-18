"""Whole real terrain topology and subtraction before Float32 conversion."""

import unittest
from pathlib import Path

import numpy as np
from stair_regional_precision import boundary_vertices, encode, inspect


class RegionalPrecision(unittest.TestCase):
    def test_whole_region_keeps_actual_neighbor_boundary_exact_and_remains_conforming(
        self,
    ):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/regional-terrain-topology.npz"
        )
        with np.load(path, allow_pickle=False) as data:
            points, faces = data["xy"], data["faces"]
        anchors = boundary_vertices(faces)
        self.assertEqual(len(anchors), 106)
        origin = np.round((points.min(axis=0) + points.max(axis=0)) / 128) * 64
        peer = points[anchors].astype("f4").astype(float)
        _encoded, ordinary = encode(points, origin)
        self.assertGreater(
            float(np.linalg.norm(ordinary[anchors] - peer, axis=1).max()), 3e-5
        )
        _encoded, actual = encode(
            points, origin, anchor_ids=anchors, anchor_world_m=peer
        )
        np.testing.assert_array_equal(actual[anchors], peer)
        proof = inspect(points, faces, origin, anchor_ids=anchors, anchor_world_m=peer)
        self.assertEqual(proof["nonpositive_faces"], 0)
        self.assertEqual(proof["merged_vertices"], 0)
        self.assertTrue(proof["coverage_valid"])

    def test_unrepresentable_neighbor_does_not_silently_open_a_seam(self):
        points = np.array([[0.125, 0.25, 0.5]])
        with self.assertRaisesRegex(ValueError, "not exactly representable"):
            encode(points, np.full(3, 1e9), anchor_ids=[0], anchor_world_m=points)
        for anchors in ([0, 0], [1], [0.5], [True]):
            with self.assertRaises(ValueError):
                encode(points, np.zeros(3), anchor_ids=anchors, anchor_world_m=points)

    def test_same_whole_real_terrain_is_positive_and_conforming_in_its_region(self):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/regional-terrain-topology.npz"
        )
        with np.load(path, allow_pickle=False) as data:
            points, faces = data["xy"], data["faces"]
        before = points.copy()
        absolute = inspect(points, faces, np.zeros(2))
        origin = np.round((points.min(axis=0) + points.max(axis=0)) / 128) * 64
        regional = inspect(points, faces, origin)
        self.assertEqual((len(points), len(faces)), (1002, 1896))
        self.assertEqual(absolute["nonpositive_faces"], 1)
        self.assertEqual(regional["nonpositive_faces"], 0)
        self.assertTrue(regional["coverage_valid"])
        self.assertEqual(regional["merged_vertices"], 0)
        self.assertLessEqual(regional["maximum_vertex_displacement_m"], 8e-6)
        np.testing.assert_array_equal(points, before)

    def test_origin_is_subtracted_in_double_before_encoding_the_small_difference(self):
        origin = np.full(3, 1e9)
        points = origin + np.array([[0.125, 0.25, 0.5]])
        encoded, actual = encode(points, origin)
        np.testing.assert_array_equal(encoded, [[0.125, 0.25, 0.5]])
        np.testing.assert_array_equal(actual, points)
        with self.assertRaises(ValueError):
            encode(points, [0, 0])


if __name__ == "__main__":
    unittest.main()
