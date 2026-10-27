"""Mass conservation, independent node shader equality and horizon partitions."""

import unittest

import numpy as np
from test_urban_parent_phase import homogeneous_parent
from urban_parent_conditional import ground_stops, render_conditional
from urban_parent_phase import render_phase
from urban_parent_response import (
    ProjectedResponse,
    centre_phase_rays,
    collect_response,
    light_geometry,
)


class ParentResponseTest(unittest.TestCase):
    def setUp(self):
        self.parent = homogeneous_parent()
        self.profile = {
            "kind": "sun",
            "direction": [0.4, -0.2, 1],
            "intensity": 3.0,
            "ambient": 0.1,
        }

    def test_vertical_mass_matches_analytic_footprint_and_height_mixture(self):
        profile = self.profile | {"direction": [0, 0, 1]}
        moments, _ = collect_response(
            self.parent,
            np.array([[253.0, 255, 100]]),
            np.array([[0.0, 0, -1]]),
            profile,
            side=4,
        )
        np.testing.assert_allclose(moments[0, 0, 0], 0.85 * 0.75**2, atol=2e-15)
        np.testing.assert_allclose(moments[0, 5, 0], 1 - 0.85 * 0.75**2, atol=2e-15)
        np.testing.assert_allclose(moments[..., 0].sum(axis=1), 1, atol=2e-15)
        np.testing.assert_allclose(moments[..., 1], moments[..., 0], atol=2e-15)
        self.assertEqual(moments[0, 6, 0], 0)

    def test_nodes_reproduce_existing_shader_for_sun_and_point_light(self):
        origins = np.array([[235.0, 231, 100], [275.0, 271, 100]])
        directions = np.tile([0.1, 0.2, -1.0], (2, 1))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        profiles = [
            self.profile,
            {
                "kind": "point",
                "position": [256, 256, 200],
                "intensity": 2e4,
                "ambient": 0.002,
            },
        ]
        for profile in profiles:
            with self.subTest(profile=profile):
                moments, _ = collect_response(
                    self.parent,
                    origins,
                    directions,
                    profile,
                    side=4,
                )
                changed, _ = collect_response(
                    self.parent,
                    origins,
                    directions,
                    profile | {"intensity": 12.0, "ambient": 4.0},
                    side=4,
                )
                np.testing.assert_array_equal(moments, changed)
                field = ProjectedResponse(
                    moments.reshape(1, 2, 7, 4), light_geometry(profile)
                )
                colour, coverage = field.evaluate([[0.25, 0.5], [0.75, 0.5]], profile)
                exact, alpha, _ = render_phase(
                    self.parent,
                    origins,
                    directions,
                    profile,
                    side=4,
                )
                np.testing.assert_allclose(colour, exact, rtol=0, atol=2e-13)
                np.testing.assert_allclose(coverage, alpha, rtol=0, atol=2e-15)
                doubled, _ = field.evaluate(
                    [[0.25, 0.5], [0.75, 0.5]],
                    profile
                    | {
                        "intensity": 2 * profile["intensity"],
                        "ambient": 2 * profile["ambient"],
                    },
                )
                np.testing.assert_allclose(doubled, 2 * colour, atol=2e-13, rtol=0)

    def test_centre_phase_matches_direct_shader_and_is_packet_invariant(self):
        origins = np.array([[254.0, -32, 2]] * 3)
        directions = np.array([[0.0, 1, -0.005], [0, 1, 0], [0, 1, 0.1]])
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        moments, _ = collect_response(
            self.parent,
            origins,
            directions,
            self.profile,
            side=4,
            phase="centre",
        )
        streamed, _ = collect_response(
            self.parent,
            origins,
            directions,
            self.profile,
            side=4,
            phase="centre",
            batch=1,
        )
        np.testing.assert_array_equal(moments, streamed)
        np.testing.assert_allclose(moments[..., 0].sum(axis=1), 1, atol=2e-15)
        self.assertTrue(np.all(moments[..., 1] <= moments[..., 0] + 2e-15))
        self.assertEqual(moments[0, 6, 0], 0)
        self.assertGreater(moments[2, 6, 0], 0)
        field = ProjectedResponse(
            moments.reshape(1, 3, 7, 4), light_geometry(self.profile)
        )
        colour, coverage = field.evaluate(
            np.c_[(np.arange(3) + 0.5) / 3, [0.5] * 3], self.profile
        )
        rays, at, weights = centre_phase_rays(self.parent, origins, directions, 4)
        direct, alpha, _ = render_conditional(self.parent, rays, at, self.profile)
        sky = ~np.isfinite(ground_stops(rays, at))
        direct[sky] += (1 - alpha[sky, None]) * 0.1
        expected = np.einsum("nsc,s->nc", direct.reshape(3, 16, 3), weights)
        np.testing.assert_allclose(colour, expected, rtol=0, atol=2e-13)
        np.testing.assert_allclose(coverage, alpha.reshape(3, 16) @ weights, atol=2e-15)

    def test_clamped_interpolation_conserves_constant_partition_and_checks_light(self):
        moments = np.zeros((2, 3, 7, 4))
        moments[..., 0, :2] = 0.4
        moments[..., 5, :2] = 0.6
        field = ProjectedResponse(moments, light_geometry(self.profile))
        uv = [[-2, -3], [0, 0], [0.5, 0.5], [1, 1], [3, 4]]
        samples = field.sample(uv)
        np.testing.assert_allclose(samples[..., 0].sum(axis=1), 1, atol=2e-15)
        self.assertTrue(np.all(samples >= 0))
        with self.assertRaisesRegex(ValueError, "rebuild"):
            field.evaluate(uv, self.profile | {"direction": [1, 0, 1]})
        with self.assertRaisesRegex(ValueError, "coordinates"):
            field.sample([[np.nan, 0]])
        with self.assertRaisesRegex(ValueError, "response"):
            ProjectedResponse(moments * np.nan, light_geometry(self.profile))


if __name__ == "__main__":
    unittest.main()
