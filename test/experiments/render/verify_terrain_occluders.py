"""Lower terrain envelopes preserve their footprint and remain below every fine triangle."""

import unittest

import numpy as np
from terrain_occluders import lower_envelopes, bracketed_proxies
from terrain_grid_lod import triangles


def patch(height):
    y, x = np.indices(height.shape, dtype=float)
    return np.stack((x, y, height), axis=-1).reshape(-1, 3)


class TerrainOccluders(unittest.TestCase):
    def test_affine_heightfield_is_represented_by_the_same_plane(self):
        y, x = np.indices((9, 9))
        points = patch(100 + 3 * x - 4 * y)[None]
        proxy, valid, lower, _ = lower_envelopes(points, 9)
        self.assertTrue(valid.all())
        np.testing.assert_allclose(lower, points[..., 2], rtol=0, atol=1e-10)
        np.testing.assert_array_equal(proxy[..., :2], points[:, [0, 8, 80, 72], :2])

    def test_interior_valley_makes_corner_only_depth_an_unsafe_occluder(self):
        height = np.full((9, 9), 10.0)
        height[4, 4] = -5
        points = patch(height)[None]
        self.assertGreater(points[0, 0, 2], points[0, 40, 2])
        proxy, valid, lower, _ = lower_envelopes(points, 9)
        self.assertTrue(valid.all())
        self.assertTrue(np.all(proxy[..., 2] <= -5))
        self.assertTrue(np.all(lower <= points[..., 2]))

    def test_random_heightfields_enclose_triangle_interiors(self):
        rng = np.random.default_rng(515)
        points = np.array([patch(h) for h in rng.normal(size=(40, 9, 9))])
        proxy, valid, lower, _ = lower_envelopes(points, 9)
        self.assertTrue(valid.all())
        self.assertTrue(np.all(lower <= points[..., 2]))
        faces = triangles(9, 1)
        for _ in range(20):
            weights = rng.dirichlet(np.ones(3), size=len(faces))
            source = np.einsum("ntk,tk->nt", points[:, faces, 2], weights)
            floor = np.einsum("ntk,tk->nt", lower[:, faces], weights)
            self.assertTrue(np.all(floor <= source + 1e-14))
        self.assertTrue(np.isfinite(proxy).all())

    def test_nonconvex_footprint_is_not_an_occluder(self):
        points = patch(np.zeros((9, 9)))[None]
        points[0, 80, :2] = (1, 1)
        self.assertFalse(lower_envelopes(points, 9)[1][0])

    def test_boundary_bulges_cannot_create_coverage_outside_the_source(self):
        points = patch(np.zeros((9, 9)))[None]
        points[0, 4, 1] -= 0.1
        points[0, 76, 1] -= 0.1
        self.assertFalse(lower_envelopes(points, 9)[1][0])

    def test_folded_grid_is_not_an_occluder(self):
        points = patch(np.zeros((9, 9)))[None]
        points[0, 40, :2] = (0, 0)
        self.assertFalse(lower_envelopes(points, 9)[1][0])

    def test_side_entry_below_the_surface_cannot_occlude(self):
        height = np.full((9, 9), 10.0)
        height[4, 4] = 0
        proxy, valid, _, gap = lower_envelopes(patch(height)[None], 9)
        self.assertTrue(valid[0])
        eye = np.array([-5.0, 4.0, 2.0])
        self.assertGreater(eye[2], proxy[0, 0, 2])
        _, sizes = bracketed_proxies(proxy, gap, eye)
        self.assertEqual(sizes[0], 0)

    def test_clipped_ray_brackets_stay_inside_the_same_footprint(self):
        rng = np.random.default_rng(515)
        points = np.array([patch(h) for h in rng.normal(size=(40, 9, 9))])
        proxy, valid, lower, gap = lower_envelopes(points, 9)
        self.assertTrue(valid.all())
        eye = np.array([-10.0, -10.0, 30.0])
        clipped, sizes = bracketed_proxies(proxy, gap, eye)
        self.assertTrue(np.all((sizes >= 3) & (sizes <= 8)))
        for i, size in enumerate(sizes):
            a, b = proxy[i, 1] - proxy[i, 0], proxy[i, 3] - proxy[i, 0]
            gradient = np.linalg.solve(np.stack((a[:2], b[:2])), [a[2], b[2]])
            above = eye[2] - proxy[i, 0, 2] - (eye[:2] - proxy[i, 0, :2]) @ gradient
            upper = eye[:2] + (1 - gap[i] / above) * (clipped[i, :size, :2] - eye[:2])
            self.assertTrue(
                np.all(
                    (clipped[i, :size, :2] >= -1e-12)
                    & (clipped[i, :size, :2] <= 8 + 1e-12)
                )
            )
            self.assertTrue(np.all((upper >= -1e-12) & (upper <= 8 + 1e-12)))
            self.assertTrue(np.all(lower[i] <= points[i, :, 2]))

    def test_incomplete_patch_is_rejected(self):
        with self.assertRaises(ValueError):
            lower_envelopes(np.zeros((1, 80, 3)), 9)


if __name__ == "__main__":
    unittest.main()
