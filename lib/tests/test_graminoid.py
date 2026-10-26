"""Independent grass mesh topology, ancestry, physical bounds and seed controls."""

import unittest

import numpy as np
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh


class GraminoidTests(unittest.TestCase):
    def test_seeded_tuft_has_positive_faces_unit_normals_and_submetre_bounds(self):
        a, b = graminoid_tuft_mesh(17), graminoid_tuft_mesh(17)
        for k in a:
            np.testing.assert_array_equal(a[k], b[k])
        p = a["vertices"][a["faces"]]
        cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        self.assertTrue((np.linalg.norm(cross, axis=1) > 0).all())
        n = a["normals"].reshape(-1, 3, 3)
        self.assertTrue((np.sum(n * cross[:, None, :], axis=2) > 0).all())
        np.testing.assert_allclose(np.linalg.norm(n, axis=2), 1, atol=1e-12)
        self.assertTrue(np.max(a["vertices"][:, 2]) < 0.5)
        self.assertTrue(np.max(a["vertices"][:, 2]) > 0.2)
        self.assertGreater(a["vertices"][:, 2].min(), -0.02)
        self.assertFalse(
            np.array_equal(graminoid_tuft_mesh(18)["vertices"], a["vertices"])
        )

    def test_shared_blade_ids_and_surface_points_survive_subdivision(self):
        coarse, fine = (
            graminoid_tuft_mesh(17, segments=4),
            graminoid_tuft_mesh(17, segments=8),
        )
        lookup = {
            (int(i), *uv): p
            for i, uv, p in zip(
                fine["blade_index"], fine["uv"], fine["vertices"], strict=True
            )
        }
        for i, uv, p in zip(
            coarse["blade_index"], coarse["uv"], coarse["vertices"], strict=True
        ):
            np.testing.assert_allclose(p, lookup[(int(i), *uv)], atol=1e-15, rtol=0)
        with self.assertRaises(ValueError):
            graminoid_tuft_mesh(-1)
        with self.assertRaises(ValueError):
            graminoid_tuft_mesh(1, blades=1000000)

    def test_blade_prefix_is_unchanged_when_group_grows(self):
        short = graminoid_tuft_mesh(17, blades=3)
        long = graminoid_tuft_mesh(17, blades=12)
        for key in short:
            np.testing.assert_array_equal(short[key], long[key][: len(short[key])])

    def test_invalid_types_and_limits_remain_bounded(self):
        for args in ((True,), (2**64,), (17, 0), (17, 65), (17, 2, 1), (17, 2, 1025)):
            with self.assertRaises(ValueError):
                graminoid_tuft_mesh(*args)


if __name__ == "__main__":
    unittest.main()
