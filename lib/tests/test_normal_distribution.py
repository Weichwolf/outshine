"""Finite-measure diffuse bounds independent of procedural geometry or renderers."""

import unittest

import numpy as np
from outshine_ref.vegetation.normal_distribution import (
    evaluate_diffuse,
    jensen_error_bound,
    mass_preserving_error_bound,
    mass_preserving_vectors,
    normal_partition_ids,
    quantization_error_bound,
)


class NormalDistributionTests(unittest.TestCase):
    def test_sharp_jensen_gap_and_no_negative_or_grazing_leak(self):
        moments = np.array([[[1.0, 0, 0, np.sqrt(0.96)]]])
        result = evaluate_diffuse(moments[..., 1:], np.eye(3))
        np.testing.assert_array_equal(result[:2, 0], [0, 0])
        self.assertAlmostEqual(float(jensen_error_bound(moments)[0]), 0.1)
        np.testing.assert_array_equal(
            evaluate_diffuse(moments[..., 1:], [[0, 0, -1.0]]), [[0]]
        )

    def test_nested_cube_partitions_and_owned_response(self):
        normals = np.array([[0.8, 0.6, 0], [-0.6, 0.8, 0], [0, -0.8, 0.6]])
        coarse = normal_partition_ids(normals, 6, np.eye(3))
        fine = normal_partition_ids(normals, 24, np.eye(3))
        np.testing.assert_array_equal(fine // 4, coarse)
        vectors = np.array([[[0.2, 0.3, 0.1]]])
        response = evaluate_diffuse(vectors, np.eye(3))
        vectors[:] = 0
        np.testing.assert_allclose(response[:, 0], [0.2, 0.3, 0.1])

    def test_mass_preserving_field_keeps_isotropic_energy_and_bounded_error(self):
        moments = np.array([[[0.7, 0.25, 0.25, 0.3], [0.3, -0.2, 0.1, 0]]])
        vectors = mass_preserving_vectors(moments.tolist())
        self.assertAlmostEqual(float(np.linalg.norm(vectors, axis=-1).sum()), 1.0)
        self.assertTrue(
            (mass_preserving_error_bound(moments) >= jensen_error_bound(moments)).all()
        )
        zero = np.zeros((1, 1, 4))
        zero[0, 0, 0] = 1
        with self.assertRaisesRegex(ValueError, "zero-resultant"):
            mass_preserving_vectors(zero)

    def test_quantization_bound_and_invalid_encodings(self):
        vectors = np.array([[[0.123456, 0.314159, -0.223344]]])
        packed = vectors.astype(np.float16)
        directions = np.array([[1.0, 0, 0], [0, 1, 0], [0, 0, 1], [-1, 0, 0]])
        error = abs(
            evaluate_diffuse(vectors, directions) - evaluate_diffuse(packed, directions)
        )
        self.assertLessEqual(
            float((error - quantization_error_bound(vectors, packed)).max()), 1e-15
        )
        with self.assertRaises(ValueError):
            quantization_error_bound(vectors, packed[:, :, :2])
        with self.assertRaises(ValueError):
            evaluate_diffuse(vectors, [[2.0, 0, 0]])

    def test_invalid_cones_frames_and_partitions_fail(self):
        for moment in (
            [[[0.0, 1, 0, 0]]],
            [[[-1.0, 0, 0, 0]]],
            [[[1.0, np.nan, 0, 0]]],
        ):
            with self.assertRaises(ValueError):
                jensen_error_bound(moment)
        with self.assertRaises(ValueError):
            normal_partition_ids(np.eye(3), 5, np.eye(3))
        with self.assertRaises(ValueError):
            normal_partition_ids(np.eye(3), 4, np.zeros((3, 3)))
        with self.assertRaises(ValueError):
            normal_partition_ids(np.zeros((1, 3)), 6, np.eye(3))


if __name__ == "__main__":
    unittest.main()
