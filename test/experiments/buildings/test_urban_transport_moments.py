"""Analytic positivity/correlation limits and unchanged complete Ready scenes."""

import unittest

import numpy as np
from pixel_reference import integrate_fields
from test_pixel_reference import camera as spatial_camera
from test_urban_ready_pixels import fixture
from urban_ready_pixels import composed_scene, integrate, integrate_control
from urban_transport_moments import compose_moments, pack_moments


class TransportMomentsTest(unittest.TestCase):
    def test_joint_anticorrelation_is_not_a_product_of_separate_means(self):
        b = np.array([[0, 0, 0], [2, 2, 2]], float)
        fields = pack_moments(np.zeros_like(b), np.ones(2), np.array([1.0, 0]), b, 0)
        rgb, transport, _ = compose_moments(fields.mean(axis=0), b.mean(axis=0))
        np.testing.assert_array_equal(rgb, 0)
        np.testing.assert_array_equal(transport, 0)
        self.assertGreater(0.5 * float(b[:, 0].mean()), 0)

    def test_nonnegative_transfer_where_raw_difference_is_negative(self):
        c, b = np.full((1, 3), 0.01), np.full((1, 3), 0.5)
        m = pack_moments(c, np.array([0.1]), np.array([0.2]), b, 0)[0]
        dense = np.full(3, 0.1)
        raw = dense + (c[0] + m[3:6] - b[0])
        self.assertTrue(np.all(raw < 0))
        rgb, transport, _ = compose_moments(m, dense)
        np.testing.assert_allclose(rgb, 0.012, rtol=0, atol=1e-16)
        self.assertTrue(np.all((transport >= 0) & (transport <= 1)))

    def test_same_grid_matches_all_existing_ready_compositions(self):
        parent, resolved, view, profile, args = fixture()
        args.weighted_transport, args.background_samples = True, 4
        box = np.array([[637, 357], [641, 361]])
        original = {k: a.tobytes() for k, a in parent.arrays().items()}
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
            complete = composed_scene(parent, resolved, profile, state, 0.75)
            moments = composed_scene(
                parent, resolved, profile, state, 0.75, component="moments"
            )
            base = composed_scene(
                parent, resolved, profile, state, 0.75, component="background"
            )
            expected, _, old_work = integrate(
                resolved, view, box, profile, args, scene_query=complete
            )
            rgb, arrays, work = integrate_control(
                resolved, view, box, profile, args, moments, base
            )
            with self.subTest(state=state):
                np.testing.assert_allclose(rgb, expected, rtol=0, atol=5e-13)
                self.assertTrue(np.all(rgb >= 0))
                if state != "all_ready":
                    self.assertEqual(
                        work["moment_phase"]["pixel_work"]["value_channels"], 10
                    )
                    self.assertEqual(
                        work["moment_phase"]["scene_work"]["parent_work"],
                        old_work["scene_work"]["parent_work"],
                    )
                    self.assertEqual(arrays["moments"].shape, (4, 4, 10))
        self.assertEqual(original, {k: a.tobytes() for k, a in parent.arrays().items()})
        self.assertEqual(
            child,
            {k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)},
        )

    def test_black_fallback_and_full_ready_preserve_any_dense_baseline(self):
        b = np.zeros((2, 3))
        fields = pack_moments(b, np.ones(2), np.ones(2), b, 0).mean(axis=0)
        dense = np.array([0.0, 2, 10])
        rgb, weight, fallback = compose_moments(fields, dense)
        np.testing.assert_array_equal(rgb, dense)
        np.testing.assert_array_equal(weight, 1)
        self.assertEqual(fallback, 3)
        parent, resolved, view, profile, args = fixture()
        args.state, args.weighted_transport, args.background_samples = (
            "all_ready",
            True,
            8,
        )
        base = composed_scene(
            parent, resolved, profile, args.state, 0.75, component="background"
        )

        def forbidden(*unused):
            raise AssertionError("explicit full Ready identity needs no moments")

        rgb, arrays, work = integrate_control(
            resolved,
            view,
            np.array([[637, 357], [641, 361]]),
            profile,
            args,
            forbidden,
            base,
        )
        np.testing.assert_array_equal(rgb, arrays["background"])
        np.testing.assert_array_equal(arrays["weighted_transport"], 1)
        self.assertNotIn("moments", arrays)
        self.assertEqual(work["moment_phase"]["pixel_work"]["rays"], 0)

    def test_ten_fields_have_analytic_means_and_crop_packet_identity(self):
        def scene(o, d):
            xy = d[:, :2] / d[:, 2, None]
            c = np.c_[xy, np.ones(len(o))]
            b = np.tile([4.0, 8, 12], (len(o), 1))
            return pack_moments(
                c, np.full(len(o), 0.25), np.full(len(o), 0.5), b, 0
            ), np.ones(len(o))

        box = np.array([[3, 5], [7, 8]])
        full, _, work = integrate_fields(
            box, spatial_camera, scene, channels=10, samples=4, batch=127
        )
        y, x = np.indices((3, 4))
        np.testing.assert_allclose(
            full[..., :3],
            np.stack([x + 3.5, y + 5.5, np.ones_like(x)], axis=-1),
            rtol=0,
            atol=2e-14,
        )
        np.testing.assert_array_equal(
            full[..., 3:6], np.broadcast_to([0.5, 1, 1.5], (3, 4, 3))
        )
        crop = integrate_fields(
            [[4, 6], [7, 8]], spatial_camera, scene, channels=10, samples=4, batch=17
        )[0]
        np.testing.assert_array_equal(crop, full[1:, 1:])
        self.assertEqual(work["scene_value_logical_B"], 12 * 16 * 10 * 8)

    def test_invalid_transport_or_unbounded_fields_are_rejected_without_clamping(self):
        c, b = np.zeros((1, 3)), np.ones((1, 3))
        with self.assertRaises(ValueError):
            pack_moments(c, np.array([1.1]), np.ones(1), b, 0)
        valid = pack_moments(c, np.ones(1), np.ones(1), b, 0)[0]
        for index, value in [(0, -0.01), (3, 2), (9, 1.1)]:
            bad = valid.copy()
            bad[index] = value
            with self.assertRaises(ValueError):
                compose_moments(bad, np.ones(3))

        def forbidden(*unused):
            raise AssertionError("invalid field count must fail before queries")

        for channels in (0, 33):
            with self.assertRaises(ValueError):
                integrate_fields(
                    [[0, 0], [1, 1]], forbidden, forbidden, channels=channels
                )

    def test_positive_normalization_does_not_certify_quadrature_accuracy(self):
        # B(x)=1+x, T=x, S=1, C=0 on [0,1]. A single midpoint has R=3/4;
        # exact mean B=3/2 but exact mean R=1/2+1/3=5/6.
        m = pack_moments(
            np.zeros((1, 3)), np.array([0.5]), np.ones(1), np.full((1, 3), 1.5), 0
        )[0]
        rgb, _, _ = compose_moments(m, np.full(3, 1.5))
        np.testing.assert_array_equal(rgb, 0.75)
        self.assertGreater(abs(float(rgb[0]) - 5 / 6), 0.08)

        # B(x)=1+x², R(x)=x, T=x/(1+x²). Midpoint R already integrates
        # exactly to1/2; replacing midpoint B=5/4 by exact mean4/3 introduces
        # a normalization error: (1/2)/(5/4)*(4/3)=8/15 instead of1/2.
        m = pack_moments(
            np.zeros((1, 3)), np.array([0.4]), np.ones(1), np.full((1, 3), 1.25), 0
        )[0]
        rgb, _, _ = compose_moments(m, np.full(3, 4 / 3))
        np.testing.assert_allclose(rgb, 8 / 15, rtol=0, atol=1e-16)
        self.assertGreater(abs(float(rgb[0]) - 0.5), 0.03)


if __name__ == "__main__":
    unittest.main()
