"""Analytic area controls, signed values and crop/packet independent samples."""

import subprocess
import sys
import unittest

import numpy as np
from pixel_reference import integrate_pixels


def camera(pixel, samples, stream):
    x, y = np.meshgrid(np.arange(samples), np.arange(samples))
    jitter = np.full((2, samples, samples), 0.5)
    if stream is not None:
        jitter = np.random.default_rng(stream).random((2, samples, samples))
    screen = np.stack(
        (
            pixel[0] + (x + jitter[0]) / samples,
            pixel[1] + (y + jitter[1]) / samples,
            np.ones_like(x),
        ),
        axis=-1,
    )
    return np.zeros((samples**2, 3)), screen.reshape(-1, 3)


def scene(origins, directions):
    x, y = (directions[:, :2] / directions[:, 2, None]).T
    return np.c_[x * x - y, y * x + 1, -x - y], np.full(len(x), 0.25)


class PixelReferenceTest(unittest.TestCase):
    def test_fixed_strata_linear_moments_and_signed_values(self):
        def linear(o, d):
            xy = d[:, :2] / d[:, 2, None]
            return np.c_[xy, -np.ones(len(d))], np.full(len(d), 0.25)

        rgb, alpha, work = integrate_pixels(
            [[3, 5], [7, 8]], camera, linear, samples=4, batch=19
        )
        y, x = np.indices((3, 4))
        expected = np.stack((x + 3.5, y + 5.5, np.full_like(x, -1)), axis=-1)
        np.testing.assert_allclose(rgb, expected, rtol=0, atol=2e-14)
        np.testing.assert_array_equal(alpha, 0.25)
        self.assertEqual(work["rays"], 12 * 16)
        self.assertEqual(work["camera_to_packet_copy_B"], 12 * 16 * 48)
        self.assertLessEqual(work["max_packet_rays"], 19)

    def test_rectangle_area_is_exact_when_edges_follow_spatial_strata(self):
        def rectangle(o, d):
            xy = d[:, :2] / d[:, 2, None]
            hit = np.all((xy >= 0.25) & (xy < 0.75), axis=1)
            return hit[:, None] * np.array([-4.0, 2, 1]), hit.astype(float)

        for seed in (None, 1113, 1729):
            rgb, alpha, _ = integrate_pixels(
                [[0, 0], [1, 1]], camera, rectangle, samples=4, seed=seed, batch=7
            )
            np.testing.assert_array_equal(rgb, [[[-1, 0.5, 0.25]]])
            np.testing.assert_array_equal(alpha, 0.25)

    def test_crop_and_packet_changes_preserve_the_same_pixel_integrals(self):
        full = integrate_pixels(
            [[3, 5], [7, 8]], camera, scene, samples=9, seed=1113, batch=127
        )
        for batch in (1, 7, 53, 2048):
            actual = integrate_pixels(
                [[4, 6], [7, 8]], camera, scene, samples=9, seed=1113, batch=batch
            )
            np.testing.assert_array_equal(actual[0], full[0][1:, 1:])
            np.testing.assert_array_equal(actual[1], full[1][1:, 1:])

    def test_matches_existing_uniform_image_rays_without_changing_the_renderer(self):
        from triangle_image_reference import average_samples, image_rays

        view = (np.array([3.0, 4, 5]), np.eye(3), 1000.0, 1280, 720)
        box = np.array([[603, 351], [608, 354]])

        def rays(pixel, samples, stream):
            p = np.asarray(pixel)
            return image_rays(
                view, np.array([p, p + 1]), 1, 1, samples, jitter_seed=stream
            )

        for samples in (1, 4, 16):
            o, d = image_rays(view, box, 5, 3, samples)
            d /= np.linalg.norm(d, axis=1)[:, None]
            expected = average_samples(scene(o, d)[0], 5, 3, samples)
            actual = integrate_pixels(box, rays, scene, samples=samples, batch=19)[0]
            np.testing.assert_allclose(actual, expected, rtol=0, atol=3e-14)

    def test_large_pixel_sample_count_is_split_before_scene_queries(self):
        sizes = []

        def checked(o, d):
            sizes.append(len(o))
            return np.full_like(o, 0.5), np.ones(len(o))

        rgb, alpha, work = integrate_pixels(
            [[1, 1], [3, 2]], camera, checked, samples=32, seed=1, batch=53
        )
        np.testing.assert_array_equal(rgb, 0.5)
        np.testing.assert_array_equal(alpha, 1)
        self.assertEqual(sum(sizes), 2048)
        self.assertEqual(work["rays"], 2048)
        self.assertLessEqual(max(sizes), 53)

    def test_empty_regions_request_neither_camera_nor_scene(self):
        def forbidden(*args):
            raise AssertionError("empty integration")

        rgb, alpha, work = integrate_pixels([[4, 5], [4, 7]], forbidden, forbidden)
        self.assertEqual(rgb.shape, (2, 0, 3))
        self.assertEqual(alpha.size, 0)
        self.assertEqual(work["rays"], 0)
        self.assertEqual(work["packet_capacity_B"], 0)

    def test_invalid_inputs_fail_without_clamping_or_replacing_values(self):
        for kwargs in (
            {"samples": 0},
            {"samples": 129},
            {"batch": 0},
            {"batch": 8193},
            {"seed": -1},
        ):
            with self.assertRaises(ValueError):
                integrate_pixels([[0, 0], [1, 1]], camera, scene, **kwargs)
        for box in (
            [[0, 0], [-1, 1]],
            [[0.0, 0.0], [1.0, 1.0]],
            [[0, 0], [2**32 + 1, 1]],
        ):
            with self.assertRaises(ValueError):
                integrate_pixels(box, camera, scene)
        for bad in (np.zeros((1, 3)), np.full((1, 3), np.nan)):
            with self.assertRaises(ValueError):
                integrate_pixels(
                    [[0, 0], [1, 1]],
                    lambda *args, bad=bad: (bad, bad),
                    scene,
                    samples=1,
                )
        for rgb, alpha in (
            ([[np.nan, 0, 0]], [0]),
            ([[0, 0, 0]], [-1]),
            ([[0, 0, 0]], [1.1]),
        ):
            with self.assertRaises(ValueError):
                integrate_pixels(
                    [[0, 0], [1, 1]],
                    camera,
                    lambda *args, rgb=rgb, alpha=alpha: (rgb, alpha),
                    samples=1,
                )

    def test_integrator_import_needs_no_geometry_or_model_stack(self):
        code = """
import builtins
original=builtins.__import__
def checked(name,*args,**kwargs):
    if name.split('.')[0] in {'trimesh','shapely','torch','tensorflow'}:raise AssertionError(name)
    return original(name,*args,**kwargs)
builtins.__import__=checked
import pixel_reference
"""
        subprocess.run([sys.executable, "-B", "-W", "error", "-c", code], check=True)


if __name__ == "__main__":
    unittest.main()
