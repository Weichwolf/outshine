"""Independent visible-corner and off-screen sunlight-caster counterexamples."""

import unittest
from dataclasses import replace
from itertools import product

import numpy as np
from outshine_ref.places import PlaceCamera
from place_demand import demand, spheres
from scipy.spatial.transform import Rotation


def camera():
    return PlaceCamera(
        "control",
        0,
        0,
        0,
        0,
        0,
        False,
        0,
        0,
        90,
        (1000, 1000),
        "2026-06-21T12:00:00Z",
        "",
        1000,
    )


class DemandTests(unittest.TestCase):
    def test_cardinal_frustum_and_near_far_intersections(self):
        centers = np.array(
            ((0, 10, 0), (20, 10, 0), (0, -10, 0), (0, 1001, 0), (0, 0, 0))
        )
        result = demand(
            camera(),
            None,
            centers,
            np.array((0, 0, 0, 2, 1)),
            (0, 0, 1),
            -10,
            tolerance_m=0,
        )
        np.testing.assert_array_equal(
            result["camera"], (True, False, False, True, True)
        )

    def test_offscreen_and_behind_camera_casters_are_retained(self):
        centers = np.array(((20, 10, 10), (0, -10, 30)))
        for i, sun in enumerate(((1, 0, 1), (0, -1, 1))):
            result = demand(
                camera(), None, centers[i : i + 1], np.zeros(1), sun, 0, tolerance_m=0
            )
            self.assertFalse(result["camera"][0])
            self.assertTrue(result["sun"][0])

    def test_finite_solar_disc_keeps_penumbra_that_point_light_misses(self):
        center = np.array(((10.2, 10, 100),))
        point = demand(camera(), None, center, np.zeros(1), (0, 0, 1), 0, tolerance_m=0)
        disc = demand(
            camera(),
            None,
            center,
            np.zeros(1),
            (0, 0, 1),
            0,
            tolerance_m=0,
            sun_angular_radius_deg=0.265,
        )
        self.assertFalse(point["sun"][0])
        self.assertTrue(disc["sun"][0])
        # A physical ray inside that disk reaches x=9.9, inside the frame.
        angle = np.arctan(0.003)
        self.assertLess(np.rad2deg(angle), 0.265)
        np.testing.assert_allclose(
            center[0]
            - (100 / np.cos(angle)) * np.array((np.sin(angle), 0, np.cos(angle))),
            (9.9, 10, 0),
            atol=1e-12,
        )

    def test_every_projected_transformed_corner_and_sampled_shadow_is_kept(self):
        rng = np.random.default_rng(81)
        reference = replace(camera(), bearing_deg=37, pitch_deg=-8)
        positions = rng.uniform((-80, -80, 5), (80, 100, 60), (300, 3))
        frames = Rotation.random(300, random_state=rng).as_matrix()
        scales = rng.uniform(0.2, 2, 300)
        bounds = np.array(((-3, -2, -1), (4, 3, 8)))
        center, radius = spheres(bounds, positions, frames, scales)
        sun = np.array((1, -0.6, 1.3))
        sun /= np.linalg.norm(sun)
        selected = demand(
            reference,
            None,
            center,
            radius,
            sun,
            -20,
            tolerance_m=0,
            sun_angular_radius_deg=0.265,
        )
        corners = np.array(list(product(*zip(bounds[0], bounds[1], strict=True))))
        world = (
            positions[:, None]
            + np.einsum("nij,kj->nki", frames, corners) * scales[:, None, None]
        )

        def visible(points):
            pixel, depth = reference.project(points)
            return (
                (pixel >= 0).all(axis=-1)
                & (pixel <= 1000).all(axis=-1)
                & (depth >= 0.1)
                & (depth <= 1000)
            )

        visible_corners = visible(world).any(axis=1)
        self.assertGreater(int(visible_corners.sum()), 0)
        self.assertTrue(selected["camera"][visible_corners].all())
        any_shadow = np.zeros(300, bool)
        for fraction in np.linspace(0, 1, 21):
            travel = np.maximum(0, (world[..., 2] + 20) / sun[2]) * fraction
            any_shadow |= visible(world - travel[..., None] * sun).any(axis=1)
        self.assertTrue(selected["sun"][any_shadow].all())

    def test_invalid_geometry_and_horizon_do_not_silently_drop_shadows(self):
        with self.assertRaises(ValueError):
            spheres(
                ((0, 0, 0), (1, 1, 1)),
                np.zeros((1, 3)),
                np.zeros((1, 3, 3)),
                np.ones(1),
            )
        result = demand(
            camera(), None, np.array(((200, 10, 10),)), np.zeros(1), (1, 0, 0), -10
        )
        self.assertTrue(result["sun"].all())


if __name__ == "__main__":
    unittest.main()
