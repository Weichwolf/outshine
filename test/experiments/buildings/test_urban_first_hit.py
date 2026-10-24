"""Independent finite-world PMFs and exact rational sampling strata."""

import itertools
import unittest

import numpy as np
from test_urban_parent_conditional import brute_hit, brute_image
from urban_first_hit import sample_first_hits
from urban_parent_conditional import grid, render_conditional
from urban_parent_stats import UrbanParent
from urban_sampled_parent import render_sampled


def small_parent():
    return UrbanParent(
        np.ones((2, 2, 3)),
        np.array([0, 0, 1, 32, 16, 0.75, 0.25]),
        np.array([6.0, 12.0]),
        np.array([0.5, 0.5]),
    )


def world_events(origin, direction):
    probabilities = [0.25, 0.375, 0.375]
    cells = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
    outcomes = {}
    for states in itertools.product(range(3), repeat=4):
        mass = np.prod([probabilities[x] for x in states])
        heights = np.array([0, 6, 12])[list(states)]
        bounds = np.zeros((4, 2, 3))
        bounds[:, 0, :2] = (cells + 0.125) * 16
        bounds[:, 1, :2] = (cells + 0.875) * 16
        bounds[:, 1, 2] = heights
        if np.any(heights > 0):
            depth, normal = brute_hit(
                bounds[heights > 0], origin[None], direction[None], 0.1
            )
        else:
            depth, normal = np.array([np.inf]), np.zeros((1, 3))
        if np.isfinite(depth[0]):
            point = origin + depth[0] * direction
            cell = np.floor(point[:2] / 16).astype(int)
            idx = int(np.flatnonzero(np.all(cells == cell, axis=1))[0])
            key = (float(depth[0]), states[idx] - 1, *cell, *normal[0])
        else:
            key = (np.inf, -1, -1, -1, 0.0, 0.0, 0.0)
        outcomes[key] = outcomes.get(key, 0) + mass
    return sorted(outcomes.items())


class FirstHitTest(unittest.TestCase):
    def test_inverse_discrete_cdf_matches_81_independent_worlds(self):
        parent = small_parent()
        for origin, direction in [
            (np.array([-8.0, 8, 2]), np.array([1.0, 0.01, 0])),
            (np.array([-8.0, 8, 8]), np.array([1.0, 0.01, 0.01])),
            (np.array([8.0, 8, 50]), np.array([0.03, 0.1, -1.0])),
        ]:
            direction /= np.linalg.norm(direction)
            pmf = world_events(origin, direction)
            before = 0
            uniform = []
            for _, mass in pmf:
                uniform.append(before + mass / 2)
                before += mass
            self.assertAlmostEqual(before, 1, places=14)
            samples, _ = sample_first_hits(
                parent,
                grid(parent),
                np.tile(origin, (len(pmf), 1)),
                np.tile(direction, (len(pmf), 1)),
                np.array(uniform),
            )
            for i, (event, _) in enumerate(pmf):
                self.assertAlmostEqual(samples["depth"][i], event[0], places=12)
                self.assertEqual(samples["height_index"][i], event[1])
                np.testing.assert_array_equal(samples["cell"][i], event[2:4])
                np.testing.assert_array_equal(samples["normal"][i], event[4:])

    def test_roof_wall_tie_retains_height_labels_and_open_stop(self):
        parent = small_parent()
        o = np.tile([-8.0, 8, 8.5], (4, 1))
        d = np.tile([1.0, 0, -0.25], (4, 1))
        sample, _ = sample_first_hits(
            parent, grid(parent), o, d, np.array([0.1, 0.6, 0.9, 0.99])
        )
        np.testing.assert_allclose(sample["depth"][:3], [10, 10, 26], atol=0, rtol=0)
        np.testing.assert_array_equal(sample["height_index"], [0, 1, 1, -1])
        np.testing.assert_array_equal(
            sample["normal"][:3], [[0, 0, 1], [-1, 0, 0], [-1, 0, 0]]
        )
        stopped, _ = sample_first_hits(
            parent, grid(parent), o, d, np.full(4, 0.1), np.full(4, 10.0)
        )
        self.assertTrue(np.isinf(stopped["depth"]).all())

    def test_analytic_background_and_rational_strata_match_box_world_rgb(self):
        parent = small_parent()
        q = [0.25, 0.375, 0.375]
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
        for origin, direction, count in [
            ([8, 8, 50], [0, 0, -1], 2),
            ([-8, 8, 2], [1, 0, 0], 10),
            ([-8, 8, 8], [1, 0, 0], 13),
        ]:
            o, d = np.array([origin], float), np.array([direction], float)
            for profile in profiles:
                expected = np.zeros((1, 3))
                coverage = np.zeros(1)
                for states in itertools.product(range(3), repeat=4):
                    probability = np.prod([q[s] for s in states])
                    rgb, alpha = brute_image(
                        np.array([0, 6, 12])[list(states)], o, d, profile
                    )
                    expected += probability * rgb
                    coverage += probability * alpha
                actual, alpha, _ = render_sampled(
                    parent, o, d, profile, samples=count, seed=1113
                )
                np.testing.assert_allclose(actual, expected, atol=3e-13, rtol=0)
                np.testing.assert_allclose(alpha, coverage, atol=2e-14, rtol=0)
                reference, _, _ = render_conditional(parent, o, d, profile)
                np.testing.assert_allclose(actual, reference, atol=3e-13, rtol=0)

    def test_batch_partition_and_empty_corridor_are_preserved(self):
        parent = small_parent()
        o = np.array([[-8, y, 2] for y in [8, 16, 24, 8, 16, 24]], float)
        d = np.tile([1.0, 0, 0], (len(o), 1))
        profile = {
            "kind": "sun",
            "direction": [-0.45, -0.5, 0.7],
            "intensity": 3.0,
            "ambient": 0.1,
        }
        a, alpha, _ = render_sampled(parent, o, d, profile, samples=10, batch=1)
        b, beta, _ = render_sampled(parent, o, d, profile, samples=10, batch=5)
        np.testing.assert_allclose(a, b, atol=3e-14, rtol=0)
        np.testing.assert_array_equal(alpha, beta)
        np.testing.assert_array_equal(alpha[[1, 4]], 0)
        empty, _, _ = render_sampled(
            parent, np.empty((0, 3)), np.empty((0, 3)), profile
        )
        self.assertEqual(empty.shape, (0, 3))


if __name__ == "__main__":
    unittest.main()
