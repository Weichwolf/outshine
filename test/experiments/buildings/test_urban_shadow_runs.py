"""Run products against original DDA and independent small box worlds."""

import itertools
import unittest

import numpy as np
from test_urban_parent_conditional import brute_image
from test_urban_parent_phase import homogeneous_parent
from urban_parent_conditional import expected_shadow, grid, render_conditional
from urban_parent_stats import UrbanParent
from urban_shadow_runs import RowRunShadows


def shadow_work():
    return {"steps": 0, "cell_ray_visits": 0, "primary_condition_tests": 0}


class ShadowRunsTest(unittest.TestCase):
    def test_closed_lower_roof_contact_is_not_lost(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        product = grid(parent)
        points = np.array([[8.0, -4.0, 0.0]])
        light = np.array([[0.0, 1.0, 1.0]])
        primary = (
            np.array([[-20.0, -20.0, 30.0]]),
            np.array([[0.0, 1.0, 0.0]]),
            np.ones(1),
        )
        own = np.full((1, 2), -1)
        expected = expected_shadow(
            parent, product, points, light, primary, own, shadow_work()
        )
        actual = RowRunShadows(parent, force=True)(
            parent, product, points, light, primary, own, shadow_work()
        )
        np.testing.assert_allclose(expected, 0.15, atol=2e-15)
        np.testing.assert_allclose(actual, expected, atol=2e-14, rtol=0)

    def test_nonuniform_packets_match_original_conditional_shadow(self):
        rng = np.random.default_rng(1113)
        parent = homogeneous_parent()
        parent.frame[:2] = [7.3, -2.6]
        parent.centre_fractions = rng.random((4, 4, 3))
        product = grid(parent)
        query = RowRunShadows(parent, force=True)
        points = np.c_[rng.uniform(-32, 544, (90, 2)), rng.uniform(0.001, 47.999, 90)]
        eye = np.broadcast_to([200.0, -100.0, 150.0], points.shape).copy()
        direction = points - eye
        limit = np.linalg.norm(direction, axis=1)
        direction /= limit[:, None]
        primary = (eye - product["reference_origin_m"], direction, limit)
        own = np.full((len(points), 2), -1)
        for light in [
            [0.0, 0.0, 1.0],
            [0.45, -0.5, 0.7],
            [-0.45, 0.5, 0.7],
            [0.0, 1.0, 0.01],
        ]:
            l = np.broadcast_to(light, points.shape).copy()
            l /= np.linalg.norm(l, axis=1)[:, None]
            expected = expected_shadow(
                parent, product, points, l, primary, own, shadow_work()
            )
            actual = query(parent, product, points, l, primary, own, shadow_work())
            np.testing.assert_allclose(actual, expected, rtol=0, atol=3e-12)

    def test_full_rgb_matches_independent_81_box_worlds(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        origins = np.array(
            [[x, y, 50.0] for x in [5.0, 11.0, 19.0, 27.0] for y in [-8.0, 5.0, 19.0]]
        )
        directions = np.tile([0.05, 0.45, -1.0], (len(origins), 1))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        profiles = [
            {"kind": "sun", "direction": d, "intensity": 3, "ambient": 0.1}
            for d in [[0, 0, 1], [0.45, -0.5, 0.7], [0.45, 0.5, 0.7]]
        ] + [
            {
                "kind": "point",
                "position": [16, 16, 100],
                "intensity": 3000,
                "ambient": 0.002,
            }
        ]
        probabilities = [0.15, 0.85 * 0.7, 0.85 * 0.3]
        counted = 0
        for p in profiles:
            rgb = np.zeros_like(origins)
            for states in itertools.product(range(3), repeat=4):
                weight = np.prod([probabilities[s] for s in states])
                image, _ = brute_image(
                    np.array([0, 6, 12])[list(states)], origins, directions, p
                )
                rgb += weight * image
            actual, coverage, work = render_conditional(
                parent,
                origins,
                directions,
                p,
                shadow_query=RowRunShadows(parent, force=True),
            )
            reference, alpha, _ = render_conditional(parent, origins, directions, p)
            np.testing.assert_array_equal(coverage, alpha)
            np.testing.assert_allclose(actual, rgb, rtol=0, atol=3e-12)
            np.testing.assert_allclose(actual, reference, rtol=0, atol=3e-12)
            counted += work.get("shadow_affine_runs", 0)
        self.assertGreater(counted, 0)

    def test_ready_domains_and_budget_retain_exact_fallback(self):
        parent = homogeneous_parent()
        product = grid(parent)
        points = np.array([[256.0, 256.0, 0.001]])
        light = np.array([[0.2, 0.1, 1.0]])
        primary = (
            np.array([[256.0, -20.0, 100.0]]),
            np.array([[0.0, 1.0, -0.3]]),
            np.array([300.0]),
        )
        own = np.full((1, 2), -1)
        for query, covered in [
            (RowRunShadows(parent, run_budget=0, force=True), np.empty((0, 2, 2), int)),
            (RowRunShadows(parent, force=True), np.array([[[0, 0], [2, 2]]])),
        ]:
            product["primary_covered"] = covered
            work = shadow_work()
            actual = query(parent, product, points, light, primary, own, work)
            expected = expected_shadow(
                parent, product, points, light, primary, own, shadow_work()
            )
            np.testing.assert_array_equal(actual, expected)
            self.assertEqual(work["run_fallback_receivers"], len(points))

    def test_long_affine_run_retains_source_and_gap(self):
        parent = homogeneous_parent()
        parent.frame[3] = 8192
        parent.centre_fractions = np.linspace(0.00001, 0.0001, 4)[
            None, :, None
        ] * np.ones((4, 4, 3))
        product = grid(parent)
        query = RowRunShadows(parent)
        points = np.array([[8.0, -16.0, 0.001], [15.68, -16.0, 0.001]])
        light = np.tile([0.0, 1.0, 0.001], (2, 1))
        primary = (
            np.tile([0.0, -100.0, 100.0], (2, 1)),
            np.tile([0.0, 1.0, 0.0], (2, 1)),
            np.ones(2),
        )
        own = np.full((2, 2), -1)
        work = shadow_work()
        actual = query(parent, product, points, light, primary, own, work)
        expected = expected_shadow(
            parent, product, points, light, primary, own, shadow_work()
        )
        np.testing.assert_allclose(actual, expected, rtol=0, atol=3e-12)
        self.assertEqual(actual[1], 1)
        self.assertGreater(work["gamma_cells"], 16)


if __name__ == "__main__":
    unittest.main()
