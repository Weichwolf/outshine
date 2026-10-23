"""Pixel-wise strata, unchanged transport and independent finite-world controls."""

import itertools
import unittest

import numpy as np
from test_urban_first_hit import small_parent
from test_urban_parent_layer import brute_layer
from urban_parent_conditional import grid
from urban_parent_layer import compose
from urban_parent_light import shade
from urban_pixel_samples import pixel_surface_packets
from urban_sampled_layer import render_sampled_layer


def pixel_surface_uniforms(width, height, spatial, draws, seed, batch=2048):
    return np.vstack(
        [
            p.copy()
            for p in pixel_surface_packets(
                width, height, spatial, draws, seed, batch=batch
            )
        ]
    )


class PixelSurfaceSamplesTest(unittest.TestCase):
    def test_packet_boundaries_do_not_change_pixels_and_height_is_lazy(self):
        reference = pixel_surface_uniforms(7, 3, 4, 4, 1113)
        for batch in (1, 7, 19, 71, 2048):
            np.testing.assert_array_equal(
                reference, pixel_surface_uniforms(7, 3, 4, 4, 1113, batch)
            )
        enormous = pixel_surface_packets(7, 10**12, 4, 4, 1113, batch=71)
        np.testing.assert_array_equal(next(enormous), reference[:71])
        enormous.close()

    def test_each_ray_and_each_pixel_keep_their_cdf_strata(self):
        for spatial in (1, 2, 3, 4):
            for draws in (1, 4, 13):
                width, height = 3, 2
                u = pixel_surface_uniforms(width, height, spatial, draws, 1113)
                np.testing.assert_array_equal(
                    np.floor(u * draws), np.broadcast_to(np.arange(draws), u.shape)
                )
                y, x = np.indices((height * spatial, width * spatial))
                owners = ((y // spatial) * width + x // spatial).ravel()
                bins = spatial**2 * draws
                for pixel in range(width * height):
                    np.testing.assert_array_equal(
                        np.sort(np.floor(u[owners == pixel].ravel() * bins)),
                        np.arange(bins),
                    )
                self.assertTrue(np.all((u >= 0) & (u < 1)))

    def test_seed_and_non_square_layout(self):
        u = pixel_surface_uniforms(7, 3, 4, 4, 1113)
        np.testing.assert_array_equal(u, pixel_surface_uniforms(7, 3, 4, 4, 1113))
        self.assertTrue(np.any(u != pixel_surface_uniforms(7, 3, 4, 4, 1114)))
        self.assertEqual(u.shape, (7 * 3 * 16, 4))
        for args in [(0, 1, 2, 4), (1, 1, 0, 4), (1, 1, 17, 4), (1, 1, 4, 513)]:
            with self.assertRaises(ValueError):
                pixel_surface_uniforms(*args, 1113)

    def test_supplied_uniforms_reproduce_default_across_packet_sizes(self):
        parent = small_parent()
        o = np.column_stack((np.linspace(3, 29, 12), np.full(12, 8), np.full(12, 50)))
        d = np.tile([0.0, 0, -1], (12, 1))
        stop = np.linspace(37, 50, 12)
        p = {"kind": "point", "position": [8, 8, 30], "intensity": 3000, "ambient": 0.1}
        rng = np.random.default_rng(np.random.SeedSequence(1113, spawn_key=(1,)))
        u = (np.arange(4)[None] + rng.random((12, 4))) / 4
        baseline = render_sampled_layer(parent, o, d, p, stop)
        for batch in (1, 3, 2048):
            supplied = render_sampled_layer(
                parent,
                o,
                d,
                p,
                stop,
                batch=batch,
                surface_packets=(
                    u[first : first + batch] for first in range(0, len(u), batch)
                ),
            )
            for a, b in zip(baseline[:3], supplied[:3], strict=True):
                np.testing.assert_array_equal(a, b)

    def test_identical_pixel_matches_81_box_worlds_with_one_draw_per_ray(self):
        parent = small_parent()
        o, d, stop = (
            np.array([[8.0, 8, 50]]),
            np.array([[0.0, 0, -1]]),
            np.array([50.0]),
        )
        p = {"kind": "point", "position": [8, 8, 16], "intensity": 30, "ambient": 0.01}
        expected = np.zeros(3)
        for state in itertools.product(range(3), repeat=4):
            probability = np.prod(np.array([0.25, 0.375, 0.375])[list(state)])
            rgb, _ = brute_layer(
                np.array([0, 6, 12])[list(state)],
                o,
                d,
                stop,
                p,
                np.zeros(4, bool),
                np.zeros(4, bool),
            )
            expected += probability * rgb[0]
        direct = shade(
            np.tile([8.0, 8, 0], (4, 1)),
            -np.repeat(d, 4, axis=0),
            -np.repeat(d, 4, axis=0),
            0.2,
            np.ones(4),
            p | {"ambient": 0},
        )
        old_errors = []
        for seed in range(6):
            colours = []
            for u in (None, pixel_surface_uniforms(1, 1, 2, 1, seed)):
                c, t, s, work = render_sampled_layer(
                    parent,
                    np.repeat(o, 4, axis=0),
                    np.repeat(d, 4, axis=0),
                    p,
                    np.repeat(stop, 4),
                    samples=1,
                    seed=seed,
                    surface_packets=None if u is None else [u],
                )
                colours.append(compose(c, t, s, direct, np.full((4, 3), 0.002)).mean(0))
                self.assertEqual(work["surface_samples_per_ray"], 1)
            np.testing.assert_allclose(colours[1], expected, atol=3e-14, rtol=0)
            old_errors.append(float(np.linalg.norm(colours[0] - expected)))
        self.assertGreater(max(old_errors), 1e-5)

    def test_varying_ray_integrands_can_have_higher_variance(self):
        estimates = []
        for permutation in itertools.permutations(range(4)):
            u = (np.asarray(permutation) + 0.5) / 4
            values = np.r_[u[:2] < 0.5, u[2:] >= 0.5]
            estimates.append(values.mean())
        self.assertAlmostEqual(np.mean(estimates), 0.5, places=14)
        self.assertAlmostEqual(np.var(estimates), 1 / 12, places=14)
        self.assertGreater(np.var(estimates), 0.25 / 4)

    def test_supplied_uniforms_validation_and_unused_full_ready_values(self):
        parent = small_parent()
        o, d, stop = (
            np.array([[8.0, 8, 50]]),
            np.array([[0.0, 0, -1]]),
            np.array([50.0]),
        )
        p = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3, "ambient": 0.1}
        for u in (np.zeros((1, 3)), [[np.nan] * 4], [[-0.1] * 4], [[1.0] * 4]):
            with self.assertRaises(ValueError):
                render_sampled_layer(parent, o, d, p, stop, surface_packets=[u])
        for packets in ([], [np.zeros((1, 4)), np.zeros((1, 4))]):
            with self.assertRaises(ValueError):
                render_sampled_layer(parent, o, d, p, stop, surface_packets=packets)
        whole = np.array([[[0, 0], grid(parent)["shape"]]])
        c, t, s, work = render_sampled_layer(
            parent,
            o,
            d,
            p,
            stop,
            primary_covered=whole,
            caster_covered=whole,
            surface_packets=[np.full((1, 4), np.nan)],
        )
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)
        np.testing.assert_array_equal(s, 1)
        self.assertEqual(work["packets"], 0)


if __name__ == "__main__":
    unittest.main()
