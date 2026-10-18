"""Whole real terrain topology and subtraction before Float32 conversion."""

import unittest
from pathlib import Path

import numpy as np
from stair_regional_precision import encode, inspect


class RegionalPrecision(unittest.TestCase):
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
