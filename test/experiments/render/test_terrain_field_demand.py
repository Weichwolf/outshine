"""Addressing, exact sample admission and independent perspective/Jacobian controls."""

import unittest

import numpy as np
from terrain_field_demand import (
    pack_requests,
    page_storage,
    plane_samples,
    requested_fields,
    requested_pages,
    storage,
    unpack_requests,
)


class FieldDemandTests(unittest.TestCase):
    def test_packed_addresses_preserve_native_boundary_identity(self):
        keys = np.array(
            [[-134217727, 134217726, 9], [134217726, -134217727, 0], [-1, 0, 4]],
            dtype=np.int32,
        )
        packed = pack_requests(keys)
        np.testing.assert_array_equal(unpack_requests(packed), keys)
        self.assertEqual(len(np.unique(packed)), len(keys))
        self.assertTrue(np.all(packed < 1 << 60))
        for key in ([0, 0, 10], [0, 0, -1], [-134217728, 0, 0]):
            with self.assertRaises(ValueError):
                pack_requests(np.array([key]))

    def test_faded_samples_never_need_an_address_or_a_fine_image(self):
        keys, active, lod = requested_fields([[1e200, -1e200]], [[2, 0]], [[0, 2]], 1)
        self.assertFalse(active.any())
        self.assertEqual(len(keys), 0)
        self.assertEqual(len(lod), 0)
        self.assertEqual(storage(keys)["package_bytes"], 0)
        near, _, lod = requested_fields([[-16.01, 16]], [[0, 0]], [[0, 0]], 1)
        np.testing.assert_array_equal(unpack_requests(near), [[-2, 1, 0]])
        np.testing.assert_array_equal(lod, [0])

    def test_trilinear_sampling_requests_both_levels_and_shares_scaled_fields(self):
        first, _, lod = requested_fields([[32, 48]], [[0.125, 0]], [[0, 0.125]], 1)
        np.testing.assert_array_equal(lod, [0.5])
        np.testing.assert_array_equal(unpack_requests(first), [[2, 3, 0], [2, 3, 1]])
        second, _, _ = requested_fields([[64, 96]], [[0.25, 0]], [[0, 0.25]], 2)
        np.testing.assert_array_equal(first, second)
        self.assertEqual(
            storage(first)["independent_page_texel_bytes"], 512**2 + 256**2
        )

    def test_eigenvalue_selection_matches_independent_svd(self):
        rng = np.random.default_rng(1934)
        at, dx, dy = rng.uniform(-10, 10, (3, 100, 2))
        dx, dy = dx * 0.02, dy * 0.02
        _, active, actual = requested_fields(at, dx, dy, 1)
        self.assertTrue(active.all())
        singular = np.linalg.svd(np.stack((dx, dy), axis=2), compute_uv=False)
        major, minor = singular.T
        taps = np.ceil(np.clip(major / minor, 1, 8))
        taps[major <= 0.01] = 1
        wanted = np.clip(
            np.log2(np.maximum(np.maximum(minor, major / taps) * 16, 1)) - 0.5, 0, 9
        )
        np.testing.assert_allclose(actual, wanted, atol=1e-12, rtol=0)

    def test_plane_derivatives_reproject_to_the_original_pixel_steps(self):
        width, height, altitude, fov, pitch = 80, 48, 60, 55, -6
        at, dx, dy, pixels = plane_samples(width, height, altitude, fov, pitch)
        tangent = np.tan(np.deg2rad(fov) * 0.5)
        sine, cosine = np.sin(np.deg2rad(pitch)), np.cos(np.deg2rad(pitch))

        def project(point):
            depth = -altitude * sine + point[:, 1] * cosine
            up = -altitude * cosine - point[:, 1] * sine
            return np.column_stack(
                (
                    (point[:, 0] / (depth * tangent * width / height) + 1) * width / 2
                    - 0.5,
                    (1 - up / (depth * tangent)) * height / 2 - 0.5,
                )
            )

        expected = np.column_stack((pixels % width, pixels // width))
        np.testing.assert_allclose(project(at), expected, atol=1e-12, rtol=0)
        step = 1e-4
        np.testing.assert_allclose(
            project(at + step * dx), expected + [step, 0], atol=1e-6, rtol=0
        )
        np.testing.assert_allclose(
            project(at + step * dy), expected + [0, step], atol=1e-6, rtol=0
        )

    def test_sky_and_invalid_inputs_do_not_become_assets(self):
        at, _, _, _ = plane_samples(80, 48, 60, 55, 90)
        self.assertEqual(at.shape, (0, 2))
        for scale in (0, -1, np.nan):
            with self.assertRaises(ValueError):
                requested_fields([[0, 0]], [[0, 0]], [[0, 0]], scale)
        with self.assertRaises(ValueError):
            requested_fields([[1e200, 0]], [[0, 0]], [[0, 0]], 1)
        with self.assertRaises(ValueError):
            plane_samples(0, 720, 60, 55, -6)

    def test_page_hierarchy_uses_spatial_parents_at_negative_coordinates(self):
        keys, _, _ = requested_pages([[-4.1, -0.1]], [[0.125, 0]], [[0, 0.125]], 1)
        np.testing.assert_array_equal(
            unpack_requests(keys, tile_bits=30), [[-2, -1, 0], [-1, -1, 1]]
        )
        self.assertEqual(page_storage(keys)["independent_page_texel_bytes"], 2 * 66**2)
        np.testing.assert_array_equal(
            pack_requests(unpack_requests(keys, tile_bits=30), tile_bits=30), keys
        )

    def test_anisotropic_taps_request_both_sides_of_a_page_edge(self):
        keys, _, lod = requested_pages([[7.99, 0]], [[1.5, 0]], [[0, 0.1]], 1)
        self.assertTrue(1 < lod[0] < 2)
        decoded = {tuple(key) for key in unpack_requests(keys, tile_bits=30)}
        self.assertEqual(decoded, {(0, 0, 1), (1, 0, 1), (0, 0, 2)})

    def test_one_texel_gutter_covers_every_bilinear_texel_at_page_seams(self):
        rng = np.random.default_rng(1934)
        point = np.concatenate(
            (rng.uniform(-1e6, 1e6, 100), [-64.001, -64, -0.001, 0, 63.999, 64])
        )
        tile = np.floor(point / 64).astype(int)
        local = point - tile * 64
        inside = np.floor(local + 0.5).astype(int)
        self.assertTrue(np.all(inside >= 0))
        self.assertTrue(np.all(inside + 1 < 66))
        np.testing.assert_array_equal(tile * 64 + inside - 1, np.floor(point - 0.5))

    def test_default_sampler_never_requests_a_mip_above_five_before_fading(self):
        rng = np.random.default_rng(1934)
        at, dx, dy = rng.uniform(-2, 2, (3, 2000, 2))
        pages, active, lod = requested_pages(at, dx, dy, 1)
        self.assertTrue(active.any())
        self.assertTrue((~active).any())
        self.assertTrue(np.all(lod <= 4.5))
        self.assertTrue(np.all(unpack_requests(pages, tile_bits=30)[:, 2] <= 5))


if __name__ == "__main__":
    unittest.main()
