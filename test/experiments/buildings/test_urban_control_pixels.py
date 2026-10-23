"""Equal-grid identity, preserved Ready baseline and honest signed/query costs."""

import unittest

import numpy as np
from compare_urban_pixel_reference import (
    composed_scene,
    integrate,
    integrate_control,
)
from test_urban_ready_pixels import fixture


class UrbanControlPixelsTest(unittest.TestCase):
    def test_same_grid_reproduces_every_complete_ready_state(self):
        parent, resolved, view, profile, args = fixture()
        box = np.array([[637, 357], [641, 361]])
        args.background_samples = 4
        original = {key: a.tobytes() for key, a in parent.arrays().items()}
        child = {
            k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)
        }
        for state in [
            "pending",
            "half_view",
            "half_caster",
            "half_ready",
            "all_ready",
            "foreground",
        ]:
            args.state = state
            full = composed_scene(parent, resolved, profile, state, 0.75)
            correction = composed_scene(
                parent, resolved, profile, state, 0.75, component="correction"
            )
            background = composed_scene(
                parent, resolved, profile, state, 0.75, component="background"
            )
            expected, _, _ = integrate(
                resolved, view, box, profile, args, scene_query=full
            )
            rgb, arrays, _ = integrate_control(
                resolved, view, box, profile, args, correction, background
            )
            with self.subTest(state=state):
                np.testing.assert_allclose(rgb, expected, rtol=0, atol=5e-13)
                np.testing.assert_array_equal(
                    rgb, arrays["background"] + arrays["correction"]
                )
        self.assertEqual(original, {k: a.tobytes() for k, a in parent.arrays().items()})
        self.assertEqual(
            child,
            {k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)},
        )

    def test_full_ready_preserves_dense_baseline_without_correction_queries(self):
        parent, resolved, view, profile, args = fixture()
        args.state, args.background_samples = "all_ready", 8
        box = np.array([[637, 357], [641, 361]])
        background = composed_scene(
            parent, resolved, profile, args.state, 0.75, component="background"
        )

        def forbidden(*unused):
            raise AssertionError("explicit complete Ready domain has zero correction")

        rgb, arrays, work = integrate_control(
            resolved, view, box, profile, args, forbidden, background
        )
        args.samples = 8
        expected, _, _ = integrate(
            resolved, view, box, profile, args, scene_query=background
        )
        np.testing.assert_array_equal(rgb, expected)
        np.testing.assert_array_equal(arrays["correction"], 0)
        self.assertEqual(work["correction_phase"]["pixel_work"]["rays"], 0)
        self.assertEqual(work["background_phase"]["pixel_work"]["rays"], 16 * 64)

    def test_signs_and_both_phase_query_counts_survive_packet_crop_changes(self):
        _, resolved, view, profile, args = fixture()
        args.state, args.background_samples = "half_ready", 8
        calls = {"correction": 0, "background": 0}

        def correction(o, d):
            calls["correction"] += len(o)
            return np.full_like(o, -2), np.ones(len(o)), {"parent_queries": len(o)}

        def background(o, d):
            calls["background"] += len(o)
            return np.full_like(o, 1.0), np.ones(len(o)), {"known_queries": len(o)}

        full = integrate_control(
            resolved,
            view,
            np.array([[3, 5], [7, 8]]),
            profile,
            args,
            correction,
            background,
        )
        self.assertEqual(calls, {"correction": 12 * 16, "background": 12 * 64})
        self.assertEqual(
            full[2]["correction_phase"]["scene_work"]["parent_queries"], 12 * 16
        )
        self.assertEqual(
            full[2]["background_phase"]["scene_work"]["known_queries"], 12 * 64
        )
        np.testing.assert_array_equal(full[0], -1)
        args.batch = 17
        crop = integrate_control(
            resolved,
            view,
            np.array([[4, 6], [7, 8]]),
            profile,
            args,
            correction,
            background,
        )
        np.testing.assert_array_equal(crop[0], full[0][1:, 1:])
        np.testing.assert_array_equal(crop[1]["correction"], -2)

    def test_invalid_density_or_state_fails_before_callbacks(self):
        _, resolved, view, profile, args = fixture()

        def forbidden(*unused):
            raise AssertionError("invalid control request")

        for state, samples, background_samples in [
            ("resolved", 4, 8),
            ("half_ready", 0, 8),
            ("half_ready", 4, 3),
            ("half_ready", 4, 129),
        ]:
            args.state, args.samples, args.background_samples = (
                state,
                samples,
                background_samples,
            )
            with self.assertRaises(ValueError):
                integrate_control(
                    resolved,
                    view,
                    np.array([[0, 0], [1, 1]]),
                    profile,
                    args,
                    forbidden,
                    forbidden,
                )


if __name__ == "__main__":
    unittest.main()
