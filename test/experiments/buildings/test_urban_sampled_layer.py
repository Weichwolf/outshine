"""Finite box-world expectations and independent view/caster replacement domains."""

import itertools
import unittest

import numpy as np
from compare_urban_layers import child_visibility
from test_urban_first_hit import small_parent
from test_urban_parent_layer import brute_layer
from test_urban_parent_phase import homogeneous_parent
from test_urban_patches import example
from urban_parent_conditional import grid
from urban_parent_layer import compose, correction, render_layer
from urban_parent_light import shade
from urban_parent_reference import prepare_resolved
from urban_prefix_first_hit import PrefixFirstHits
from urban_prefix_runs import PrefixRuns
from urban_sampled_layer import render_sampled_layer
from urban_sampled_parent import render_sampled


def backdrop(o, d, stop, profile):
    points = o + stop[:, None] * d
    n = np.tile([0.0, 0, 1], (len(o), 1))
    direct = shade(points, n, -d, 0.2, np.ones(len(o)), profile | {"ambient": 0})
    return direct, np.full_like(o, profile["ambient"] * 0.2)


class SampledLayerTest(unittest.TestCase):
    def test_nonuniform_T_and_conditional_receiver_shadow_match_complete_control(self):
        parent = homogeneous_parent()
        parent.frame[:2], parent.frame[3] = [7.3, -2.6], 128
        rng = np.random.default_rng(1118)
        parent.centre_fractions = rng.uniform(0.001, 1, (4, 4, 3))
        o = rng.uniform([-20, -40, 60], [160, -32, 100], (60, 3))
        target = rng.uniform([0, 0, 0], [128, 128, 10], (60, 3))
        d = target - o
        stop = np.linalg.norm(d, axis=1)
        d /= stop[:, None]
        regions = np.array([[[0, 0], [4, 8]]])
        profiles = [
            {
                "kind": "sun",
                "direction": [-0.45, -0.5, 0.7],
                "intensity": 3.0,
                "ambient": 0.1,
            },
            {
                "kind": "point",
                "position": [60, 60, 150],
                "intensity": 3000.0,
                "ambient": 0.002,
            },
        ]
        for p in profiles:
            for view, caster in [
                (None, None),
                (regions, None),
                (None, regions),
                (regions, regions),
            ]:
                options = {"primary_covered": view, "caster_covered": caster}
                _, t, s, _ = render_sampled_layer(parent, o, d, p, stop, **options)
                _, expected_t, expected_s, _ = render_layer(
                    parent, o, d, p, stop, **options
                )
                np.testing.assert_allclose(t, expected_t, rtol=3e-12, atol=3e-14)
                np.testing.assert_allclose(s, expected_s, rtol=3e-11, atol=3e-13)

    def test_ready_caster_contains_background_point_without_crashing(self):
        product = prepare_resolved(example())
        p = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3.0, "ambient": 0.1}
        points = np.array([[8.0, 8, 0], [8, 8, 6], [0.5, 8, 0]])
        normals = np.tile([0.0, 0, 1], (len(points), 1))
        work = {}
        visibility = child_visibility(product, points, normals, p, work)
        np.testing.assert_array_equal(visibility, [0, 1, 1])
        self.assertEqual(work["near_limit_hits"], 1)

    def test_81_world_rgb_expectations_and_independent_ready_domains(self):
        parent = small_parent()
        regions = np.array([[[0, 0], [1, 2]]])
        mask = np.array([True, False, True, False])
        profiles = [
            {"kind": "sun", "direction": v, "intensity": 3.0, "ambient": 0.1}
            for v in [[0, 0, 1], [-0.45, -0.5, 0.7], [0.45, 0.5, 0.7]]
        ]
        profiles.append(
            {
                "kind": "point",
                "position": [-20, -30, 100],
                "intensity": 3000.0,
                "ambient": 0.002,
            }
        )
        for origin, direction, stop, count in [
            ([8, 8, 50], [0, 0, -1], 50, 2),
            ([8, 8, 50], [0, 0, -1], 42, 2),
            ([-8, 8, 2], [1, 0, 0], 40, 10),
            ([-8, 8, 8], [1, 0, 0], 40, 13),
        ]:
            o, d, stops = (
                np.array([origin], float),
                np.array([direction], float),
                np.array([stop], float),
            )
            for profile in profiles:
                for view, caster in [
                    (False, False),
                    (True, False),
                    (False, True),
                    (True, True),
                ]:
                    rgb, alpha = np.zeros_like(o), np.zeros(len(o))
                    for state in itertools.product(range(3), repeat=4):
                        weight = np.prod(np.array([0.25, 0.375, 0.375])[list(state)])
                        c, a = brute_layer(
                            np.array([0, 6, 12])[list(state)],
                            o,
                            d,
                            stops,
                            profile,
                            mask & view,
                            mask & caster,
                        )
                        rgb += weight * c
                        alpha += weight * a
                    c, t, s, _ = render_sampled_layer(
                        parent,
                        o,
                        d,
                        profile,
                        stops,
                        samples=count,
                        primary_covered=regions if view else None,
                        caster_covered=regions if caster else None,
                    )
                    direct, other = backdrop(o, d, stops, profile)
                    np.testing.assert_allclose(1 - t, alpha, atol=2e-14, rtol=0)
                    np.testing.assert_allclose(
                        compose(c, t, s, direct, other), rgb, atol=3e-13, rtol=0
                    )

    def test_full_ready_bypasses_parent_and_preserves_signed_baseline(self):
        parent = small_parent()
        whole = np.array([[[0, 0], grid(parent)["shape"]]])
        o, d = np.array([[8.0, 8, 50]]), np.array([[0.0, 0, -1]])
        profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 3.0,
            "ambient": 0.1,
        }

        def forbidden(*_):
            raise AssertionError("full-ready parent must not request external shadows")

        c, t, s, work = render_sampled_layer(
            parent,
            o,
            d,
            profile,
            np.array([30.0]),
            primary_covered=whole,
            caster_covered=whole,
            external_visibility=forbidden,
            receiver_normals=np.array([[1.0, 0, 0]]),
        )
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)
        np.testing.assert_array_equal(s, 1)
        self.assertEqual(work["full_ready_bypass_rays"], 1)
        self.assertEqual(work["packets"], 0)
        direct, other = np.array([[3.0, 2, 1]]), np.array([[0.1, 0.2, 0.3]])
        np.testing.assert_array_equal(correction(c, t, s, direct, other), 0)
        np.testing.assert_array_equal(compose(c, t, s, direct, other), direct + other)

    def test_external_casters_shadow_only_direct_parent_light(self):
        parent = small_parent()
        o, d, stop = (
            np.array([[8.0, 8, 50]]),
            np.array([[0.0, 0, -1]]),
            np.array([50.0]),
        )
        p = {
            "kind": "sun",
            "direction": [-0.45, -0.5, 0.7],
            "intensity": 3.0,
            "ambient": 0.1,
        }
        c, t, s, _ = render_sampled_layer(
            parent,
            o,
            d,
            p,
            stop,
            external_visibility=lambda points, normals: np.zeros(len(points)),
        )
        ambient, at, ass, _ = render_sampled_layer(
            parent, o, d, p | {"intensity": 0}, stop
        )
        np.testing.assert_array_equal(c, ambient)
        np.testing.assert_array_equal(t, at)
        np.testing.assert_array_equal(s, ass)
        for value in [np.nan, -0.1, 1.1]:
            with self.assertRaises(ValueError):
                render_sampled_layer(
                    parent,
                    o,
                    d,
                    p,
                    stop,
                    external_visibility=lambda points, normals, value=value: np.full(
                        len(points), value
                    ),
                )

    def test_receiver_normals_open_roof_stop_and_infinite_sky(self):
        parent = small_parent()
        o, d = np.array([[8.0, 8, 50], [-8, 8, 2]]), np.array([[0.0, 0, -1], [1, 0, 0]])
        p = {
            "kind": "sun",
            "direction": [-0.45, -0.5, 0.7],
            "intensity": 3.0,
            "ambient": 0.1,
        }
        stop, normals = np.array([38.0, 10.0]), np.tile([0.0, 0, 1], (2, 1))
        normals[1] = [-1, 0, 0]
        a = render_sampled_layer(parent, o, d, p, stop, receiver_normals=normals)
        b = render_layer(parent, o, d, p, stop, receiver_normals=normals)
        for x, y in zip(a[:3], b[:3], strict=True):
            np.testing.assert_allclose(x, y, atol=3e-14, rtol=0)
        np.testing.assert_array_equal(a[0], 0)
        sky = render_sampled_layer(parent, o, d, p, np.full(2, np.inf))
        np.testing.assert_array_equal(sky[2], 1)

    def test_ground_and_batch_partition_preserve_previous_sampled_output(self):
        parent = small_parent()
        o, d = np.tile([8.0, 8, 50], (5, 1)), np.tile([0.0, 0, -1], (5, 1))
        p = {
            "kind": "sun",
            "direction": [-0.45, -0.5, 0.7],
            "intensity": 3.0,
            "ambient": 0.1,
        }
        stop = np.full(len(o), 50.0)
        expected, alpha, _ = render_sampled(parent, o, d, p, samples=4)
        for batch in [1, 5]:
            c, t, s, _ = render_sampled_layer(
                parent, o, d, p, stop, samples=4, batch=batch
            )
            points, normals = o + stop[:, None] * d, np.tile([0.0, 0, 1], (len(o), 1))
            actual = c + t[:, None] * shade(points, normals, -d, 0.2, s, p)
            np.testing.assert_array_equal(actual, expected)
            np.testing.assert_array_equal(1 - t, alpha)

    def test_masked_prefix_and_prefix_sampler_use_original_categorical_probabilities(
        self,
    ):
        parent = small_parent()
        model = PrefixRuns(parent)
        model.product["primary_covered"] = np.array([[[0, 0], [1, 2]]])
        o, d = np.array([[-8.0, 8, 2]]), np.array([[1.0, 0, 0]])
        t, work = model.transmission(o, d)
        np.testing.assert_array_equal(t, [0.25])
        self.assertGreater(work["masked_dda"]["cell_ray_visits"], 0)
        hit, _ = PrefixFirstHits(model).sample(o, d, np.array([0.1]))
        np.testing.assert_array_equal(hit["cell"], [[1, 0]])
        model.product["primary_covered"] = np.array([[[0, 0], [2, 2]]])
        t, work = model.transmission(o, d)
        np.testing.assert_array_equal(t, 1)
        self.assertEqual(work["ready_bypass_rays"], 1)

    def test_invalid_and_empty_contracts(self):
        parent = small_parent()
        o, d, stop = (
            np.array([[8.0, 8, 50]]),
            np.array([[0.0, 0, -1]]),
            np.array([50.0]),
        )
        p = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3.0, "ambient": 0.1}
        for options in [
            {"stop": np.array([np.nan])},
            {"stop": np.array([0.0])},
            {"primary_covered": np.array([[[0.0, 0], [1, 2]]])},
            {"receiver_normals": np.zeros_like(o)},
            {"samples": 0},
            {"batch": 0},
        ]:
            with self.assertRaises(ValueError):
                render_sampled_layer(parent, o, d, p, **({"stop": stop} | options))
        c, t, s, _ = render_sampled_layer(
            parent, np.empty((0, 3)), np.empty((0, 3)), p, np.empty(0)
        )
        self.assertEqual(c.shape, (0, 3))
        self.assertEqual(t.shape, (0,))
        self.assertEqual(s.shape, (0,))


if __name__ == "__main__":
    unittest.main()
