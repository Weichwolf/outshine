"""Joint pixel composition, original ray controls and preserved ready children."""

import argparse
import unittest

import numpy as np
from compare_urban_layers import layer_states, mixed
from compare_urban_parents import opaque_image
from compare_urban_pixel_reference import STATES, composed_scene, integrate
from pixel_reference import integrate_pixels
from test_pixel_reference import camera as spatial_camera
from test_urban_patches import example
from triangle_image_reference import average_samples, image_rays
from urban_parent_layer import compose, correction
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent


def fixture():
    parent = UrbanParent(
        np.ones((2, 2, 3)),
        np.array([0, 0, 1, 48, 16, 0.75, 0.15]),
        np.array([6.0, 12.0, 24.0]),
        np.array([0.6, 0.3, 0.1]),
    )
    resolved = prepare_resolved(example())
    view = (np.array([16.0, 16, 300]), np.diag([1.0, -1, -1]), 300.0, 1280, 720)
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3,
        "ambient": 0.1,
    }
    args = argparse.Namespace(samples=4, seed=None, batch=127, trace_memory=False)
    return parent, resolved, view, profile, args


class UrbanReadyPixelsTest(unittest.TestCase):
    def test_joint_transport_product_and_signed_difference_before_filtering(self):
        def scene(o, d):
            x = d[:, 0] / d[:, 2]
            t = (x < 0.5).astype(float)
            s = 1 - t
            c = np.zeros_like(o)
            direct, other = np.ones_like(o), np.zeros_like(o)
            rgb = compose(c, t, s, direct, other)
            delta = correction(c, t, s, direct, other)
            return rgb + delta, np.ones(len(o))

        rgb, _, _ = integrate_pixels(
            [[0, 0], [1, 1]], spatial_camera, scene, samples=4, batch=7
        )
        np.testing.assert_array_equal(rgb, -1)
        separately_filtered = 0.5 * 0.5 + (0.5 * 0.5 - 1)
        self.assertGreater(abs(float(rgb[0, 0, 0]) - separately_filtered), 0.4)

    def test_complete_ready_states_match_direct_existing_ray_composition(self):
        parent, resolved, view, profile, args = fixture()
        box = np.array([[637, 357], [641, 361]])
        o, d = image_rays(view, box, 4, 4, args.samples)
        d /= np.linalg.norm(d, axis=1)[:, None]
        original = {key: value.tobytes() for key, value in parent.arrays().items()}
        child = {
            k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)
        }
        opaque = opaque_image(resolved, o, d, profile)[0]
        for state in STATES:
            query = composed_scene(parent, resolved, profile, state, 0.75)
            actual, _, counts = integrate(
                resolved, view, box, profile, args, scene_query=query
            )
            if state == "resolved":
                expected = opaque
            else:
                values = next(
                    s for s in layer_states(parent, resolved) if s[0] == state
                )
                expected, _ = mixed(
                    parent, o, d, profile, *values[1:], foreground_fraction=0.75
                )
            with self.subTest(state=state):
                np.testing.assert_allclose(
                    actual, average_samples(expected, 4, 4, 4), rtol=0, atol=5e-13
                )
                if state == "all_ready":
                    np.testing.assert_allclose(expected, opaque, rtol=0, atol=2e-14)
                    self.assertEqual(
                        counts["scene_work"]["parent_work"]["cell_ray_visits"], 0
                    )
        self.assertEqual(original, {k: a.tobytes() for k, a in parent.arrays().items()})
        self.assertEqual(
            child,
            {k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)},
        )

    def test_ready_boundary_crop_and_packets_preserve_requeried_scene(self):
        parent, resolved, view, profile, args = fixture()
        args.seed = 1113
        full_box = np.array([[637, 357], [641, 361]])
        crop_box = np.array([[638, 358], [640, 360]])
        for state in ["half_view", "half_caster", "half_ready", "foreground"]:
            query = composed_scene(parent, resolved, profile, state, 0.75)
            args.batch = 127
            full = integrate(
                resolved, view, full_box, profile, args, scene_query=query
            )[0]
            args.batch = 17
            crop = integrate(
                resolved, view, crop_box, profile, args, scene_query=query
            )[0]
            with self.subTest(state=state):
                np.testing.assert_array_equal(crop, full[1:3, 1:3])


if __name__ == "__main__":
    unittest.main()
