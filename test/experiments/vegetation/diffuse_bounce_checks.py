"""Independent energy, angular and finite-area checks for diffuse neighbour exchange."""

import unittest
from contextlib import redirect_stdout
from io import StringIO

import numpy as np
from diffuse_bounce import neighbour_states
from diffuse_scatter import sample_sheet, uniform_samples
from scatter_reference import Float64Nearest, rectangle_factor
from scatter_render import render_arrays
from scatter_scene import ScatterScene


def square(z, size=1):
    return np.array(
        ((-size, -size, z), (size, -size, z), (size, size, z), (-size, size, z)), float
    ), np.array(((0, 1, 2), (0, 2, 3)))


class DiffuseBounceChecks(unittest.TestCase):
    def test_black_receiver_avoids_neighbour_geometry_work(self):
        v, f = square(1)
        scene = ScatterScene(v, f)
        state = {"leaf_rt": [0] * 6, "ground_albedo": [0] * 3, "lights": []}
        result = neighbour_states(
            scene,
            np.zeros((3, 3)),
            np.tile((0, 0, 1.0), (3, 1)),
            (0, 0, 1.0),
            np.zeros((3, 6)),
            uniform_samples(3, 0, 3),
            {"test": state},
            2,
            2e-6,
        )
        np.testing.assert_array_equal(result["test"], 0)
        self.assertEqual(scene.work["rays"], 0)

    def test_closed_form_matches_independent_area_quadrature(self):
        x, w = np.polynomial.legendre.leggauss(64)
        for height in (1.0, 2.0):
            integrand = height**2 / (
                np.pi * (height**2 + x[:, None] ** 2 + x[None, :] ** 2) ** 2
            )
            actual = np.sum(integrand * w[:, None] * w[None, :])
            self.assertAlmostEqual(actual, rectangle_factor(1, 1, height), places=12)

    def test_full_ray_image_is_identical_across_packet_sizes(self):
        v, f = square(0, 3)
        state = {
            "leaf_rt": [0.1] * 3 + [0.2] * 3,
            "ground_albedo": [0.2] * 3,
            "lights": [{"direction": [0, 0, 1], "irradiance_rgb": [1] * 3}],
        }
        outputs = []
        for packet in (16, 64):
            with redirect_stdout(StringIO()):
                outputs.append(
                    render_arrays(
                        ScatterScene(v, f),
                        {"test": state},
                        0,
                        np.array((np.sqrt(3) / 2, 0, 0.5)),
                        16,
                        9,
                        4,
                        73,
                        packet,
                    )
                )
        for key in outputs[0]["test"]:
            np.testing.assert_array_equal(
                outputs[0]["test"][key], outputs[1]["test"][key]
            )

    def test_counter_samples_are_identical_across_packet_boundaries(self):
        whole = uniform_samples(37, 0, 8192)
        np.testing.assert_array_equal(
            whole,
            np.concatenate(
                [uniform_samples(37, 0, 133), uniform_samples(37, 133, 8059)]
            ),
        )
        self.assertTrue(((whole >= 0) & (whole < 1)).all())

    def test_white_furnace_energy_and_cosine_distribution(self):
        count = 65536
        optical = np.tile((0.1, 0.4, 0.2, 0.5, 0.1, 0.3), (count, 1))
        normals = np.tile((0, 0, 1.0), (count, 1))
        directions, weights = sample_sheet(
            optical, normals, (0, 0, 1.0), uniform_samples(19, 0, count)
        )
        np.testing.assert_allclose(np.linalg.norm(directions, axis=1), 1, atol=1e-14)
        np.testing.assert_allclose(
            weights.mean(0), optical[0, :3] + optical[0, 3:], atol=0.01
        )
        self.assertAlmostEqual(np.mean(abs(directions[:, 2])), 2 / 3, delta=0.01)
        opposite, same_weights = sample_sheet(
            optical, -normals, (0, 0, 1.0), uniform_samples(19, 0, count)
        )
        np.testing.assert_array_equal(directions, opposite)
        np.testing.assert_array_equal(weights, same_weights)

    def test_nearest_matches_independent_float64_for_hits_misses_and_winding(self):
        v, f = square(1)
        origins = np.tile((0.13, 0.27, 0.0), (256, 1))
        optical = np.tile((0.3,) * 3 + (0.0,) * 3, (256, 1))
        directions, _ = sample_sheet(
            optical,
            np.tile((0, 0, 1.0), (256, 1)),
            (0, 0, 1.0),
            uniform_samples(67, 0, 256),
        )
        oracle = Float64Nearest(v, f)
        expected, t, _ = oracle.query(origins, directions, 10)
        for faces in (f, f[:, ::-1]):
            actual, p, _, distance = ScatterScene(v, faces).nearest(
                origins, directions, 10
            )
            np.testing.assert_array_equal(actual >= 0, expected >= 0)
            np.testing.assert_allclose(
                distance[actual >= 0], t[expected >= 0], atol=1e-12
            )
            np.testing.assert_allclose(
                p[actual >= 0],
                origins[actual >= 0] + t[expected >= 0, None] * directions[actual >= 0],
                atol=1e-12,
            )

    def test_diffuse_transmission_has_finite_rectangle_form_factor(self):
        state = {
            "leaf_rt": [0.1] * 3 + [0.3] * 3,
            "ground_albedo": [0.2] * 3,
            "lights": [{"direction": [0, 0, 1], "irradiance_rgb": [np.pi] * 3}],
        }
        count = 65536
        optical = np.tile((0.4,) * 3 + (0.0,) * 3, (8192, 1))
        normals = np.tile((0, 0, 1.0), (8192, 1))
        points = np.zeros((8192, 3))
        for height in (1.0, 2.0):
            v, f = square(height)
            scene = ScatterScene(v, f)
            total = 0.0
            for first in range(0, count, 8192):
                answer = neighbour_states(
                    scene,
                    points,
                    normals,
                    (0, 0, 1.0),
                    optical,
                    uniform_samples(31, first, 8192),
                    {"test": state},
                    2,
                    2e-6,
                )
                total += answer["test"][:, 0].sum()
            expected = 0.4 * 0.3 * rectangle_factor(1, 1, height)
            # Six worst-case Bernoulli standard deviations, fixed before measuring.
            self.assertAlmostEqual(
                total / count, expected, delta=0.12 * 3 / np.sqrt(count) + 1e-6
            )
            self.assertGreater(abs(0.4 * 0.3 - expected), 0.05)

    def test_opaque_neighbour_has_no_transmitted_source(self):
        state = {
            "leaf_rt": [0.5] * 3 + [0] * 3,
            "ground_albedo": [0.2] * 3,
            "lights": [{"direction": [0, 0, 1], "irradiance_rgb": [1] * 3}],
        }
        v, f = square(1)
        answer = neighbour_states(
            ScatterScene(v, f),
            np.zeros((100, 3)),
            np.tile((0, 0, 1.0), (100, 1)),
            (0, 0, 1.0),
            np.tile((0.4,) * 3 + (0,) * 3, (100, 1)),
            uniform_samples(17, 0, 100),
            {"test": state},
            2,
            2e-6,
        )
        np.testing.assert_array_equal(answer["test"], 0)


if __name__ == "__main__":
    unittest.main()
