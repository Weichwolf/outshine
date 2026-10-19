"""Analytic coincident-side, perspective-depth and prepared-source controls."""

import unittest

import numpy as np
from source_far_reprojection import intersect_source, prepare_source


class TriangleReferenceTests(unittest.TestCase):
    def setUp(self):
        self.source = {
            "positions": np.array(
                [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [0.0, 1.0, 0.0]]
            ),
            "indices": np.array([[0, 2, 1], [0, 3, 2], [0, 1, 2], [0, 2, 3]]),
        }

    def test_coincident_backside_does_not_erase_frontside(self):
        origin = np.array([[0.2, 0.3, 1.0], [0.7, 0.6, 1.0]])
        direction = np.tile([0.0, 0.0, -1.0], (2, 1))
        depth, face = intersect_source(self.source, origin, direction)
        np.testing.assert_allclose(depth, [1, 1])
        self.assertTrue((face >= 2).all())

    def test_unscaled_ray_parameter_and_prepared_queries(self):
        origin = np.array([[0.1, 0.1, 2.0], [2.0, 2.0, 2.0]])
        direction = np.array([[0.2, 0.1, -2.0], [0.0, 0.0, -2.0]])
        prepared = prepare_source(self.source)
        depth, face = intersect_source(
            self.source, origin, direction, prepared=prepared
        )
        self.assertAlmostEqual(depth[0], 1)
        self.assertTrue(np.isinf(depth[1]))
        self.assertEqual(face[1], -1)
        fresh = intersect_source(self.source, origin, direction)
        np.testing.assert_array_equal(depth, fresh[0])
        np.testing.assert_array_equal(face, fresh[1])
        with self.assertRaises(ValueError):
            intersect_source(dict(self.source), origin, direction, prepared=prepared)

    def test_near_and_reversed_rays_stay_missing(self):
        # Reversed rays must select the opposite physical side, not a hidden
        # front face behind the observer. Sub-near intersections are excluded.
        origin = np.array([[0.3, 0.4, 0.05], [0.3, 0.4, 2.0]])
        direction = np.array([[0.0, 0.0, -1.0], [0.0, 0.0, 1.0]])
        depth, face = intersect_source(self.source, origin, direction)
        self.assertTrue(np.isinf(depth).all())
        np.testing.assert_array_equal(face, [-1, -1])

    def test_random_triangles_against_exhaustive_moller_trumbore(self):
        # Independent exhaustive determinant test: no Trimesh broad phase,
        # triangle-plane solver, location deduplication or Embree acceleration.
        rng = np.random.default_rng(619)
        triangles = rng.uniform(-1, 1, (100, 3, 3))
        source = {
            "positions": triangles.reshape(-1, 3),
            "indices": np.arange(300).reshape(-1, 3),
        }
        origins = np.column_stack((rng.uniform(-1, 1, (64, 2)), np.full(64, 3.0)))
        directions = np.column_stack(
            (rng.uniform(-0.2, 0.2, (64, 2)), np.full(64, -2.0))
        )
        depth, face = intersect_source(source, origins, directions)
        edge1, edge2 = (
            triangles[:, 1] - triangles[:, 0],
            triangles[:, 2] - triangles[:, 0],
        )
        expected = np.full(64, np.inf)
        owners = np.full(64, -1)
        for i, (origin, direction) in enumerate(zip(origins, directions, strict=True)):
            p = np.cross(direction, edge2)
            det = (edge1 * p).sum(1)
            inverse = np.divide(1.0, det, out=np.zeros_like(det), where=det > 1e-12)
            offset = origin - triangles[:, 0]
            u = (offset * p).sum(1) * inverse
            q = np.cross(offset, edge1)
            v = (q * direction).sum(1) * inverse
            t = (edge2 * q).sum(1) * inverse
            valid = (det > 1e-12) & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.1)
            candidates = np.where(valid, t, np.inf)
            expected[i] = candidates.min()
            if np.isfinite(expected[i]):
                owners[i] = candidates.argmin()
        self.assertGreater(np.isfinite(expected).sum(), 10)
        np.testing.assert_allclose(depth, expected, atol=1e-12)
        np.testing.assert_array_equal(face, owners)


if __name__ == "__main__":
    unittest.main()
