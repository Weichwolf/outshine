"""Exhaustive independent boxes check visibility/height/shadow correlations."""

import itertools
import unittest

import numpy as np
from urban_parent_conditional import blocker_probability, render_conditional
from urban_parent_light import light_at, shade
from urban_parent_stats import UrbanParent


def brute_hit(bounds, origins, directions, minimum):
    parallel = directions == 0
    times = np.divide(
        bounds[None] - origins[:, None, None],
        directions[:, None, None],
        out=np.zeros((len(origins), len(bounds), 2, 3)),
        where=~parallel[:, None, None],
    )
    near = np.where(parallel[:, None], -np.inf, times.min(axis=2))
    far = np.where(parallel[:, None], np.inf, times.max(axis=2)).min(axis=2)
    outside = np.any(
        parallel[:, None]
        & (
            (origins[:, None] < bounds[None, :, 0])
            | (origins[:, None] > bounds[None, :, 1])
        ),
        axis=2,
    )
    entry = np.maximum(near.max(axis=2), minimum)
    candidate = np.where((entry <= far) & ~outside, entry, np.inf)
    owner = candidate.argmin(axis=1)
    rows = np.arange(len(origins))
    depth = candidate[rows, owner]
    axis = near[rows, owner].argmax(axis=1)
    normals = np.zeros_like(origins)
    normals[rows, axis] = -np.sign(directions[rows, axis])
    return depth, normals


def brute_image(heights, origins, directions, profile):
    cells = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
    bounds = np.zeros((4, 2, 3))
    bounds[:, 0, :2] = (cells + 0.125) * 16
    bounds[:, 1, :2] = (cells + 0.875) * 16
    bounds[:, 1, 2] = heights
    active = bounds[np.asarray(heights) > 0]
    ground_time = np.divide(
        -origins[:, 2],
        directions[:, 2],
        out=np.full(len(origins), np.inf),
        where=directions[:, 2] < 0,
    )
    ground_time = np.where(ground_time >= 0.1, ground_time, np.inf)
    if len(active):
        depth, normals = brute_hit(active, origins, directions, 0.1)
    else:
        depth, normals = np.full(len(origins), np.inf), np.zeros_like(origins)
    hit = np.isfinite(depth) & (depth < ground_time)
    normals[~hit] = [0, 0, 1]
    times = np.where(hit, depth, ground_time)
    rays = np.flatnonzero(np.isfinite(times))
    points = origins[rays] + times[rays, None] * directions[rays]
    light, _, limit = light_at(points, profile)
    visibility = np.ones(len(points))
    if len(active):
        shadow, _ = brute_hit(active, points + normals[rays] * 0.001, light, 0)
        visibility = (~np.isfinite(shadow) | (shadow >= limit)).astype(float)
    rgb = np.zeros_like(origins)
    for mask, albedo in [(hit[rays], 0.6), (~hit[rays], 0.2)]:
        rgb[rays[mask]] = shade(
            points[mask],
            normals[rays[mask]],
            -directions[rays[mask]],
            albedo,
            visibility[mask],
            profile,
        )
    return rgb, hit.astype(float)


class ConditionalParentTest(unittest.TestCase):
    def test_known_miss_conditions_mutually_exclusive_heights(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        value = blocker_probability(
            parent,
            np.array([0.8, 0.8, 0.8]),
            np.array([5.0, 7.0, 5.0]),
            np.array([10.0, 10.0, np.inf]),
        )
        np.testing.assert_allclose(value, [0.56 / 0.76, 0, 0.8])

    def test_all_81_scenes_match_conditional_rgb_and_coverage(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        origins = np.array(
            [[x, y, 50] for x in [5.0, 11.0, 19.0, 27.0] for y in [-8.0, 5.0, 19.0]],
            float,
        )
        directions = np.tile([0.05, 0.45, -1.0], (len(origins), 1))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        q = 0.85
        probabilities = [1 - q, q * 0.7, q * 0.3]
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
        for profile in profiles:
            rgb, coverage = np.zeros_like(origins), np.zeros(len(origins))
            for states in itertools.product(range(3), repeat=4):
                weight = np.prod([probabilities[s] for s in states])
                a, b = brute_image(
                    np.array([0, 6, 12])[list(states)], origins, directions, profile
                )
                rgb += weight * a
                coverage += weight * b
            actual, alpha, work = render_conditional(
                parent, origins, directions, profile, batch=5
            )
            np.testing.assert_allclose(alpha, coverage, rtol=0, atol=2e-14)
            np.testing.assert_allclose(actual, rgb, rtol=0, atol=2e-13)
            self.assertGreater(work["shadow_history_cell_comparisons"], 0)

    def test_all_81_scenes_from_ground_up_down_and_horizon(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        o = np.array(
            [
                [-8.0, y, z]
                for y in [5.0, 11.0, 19.0, 27.0]
                for z in [2.0, 6.0, 7.0, 12.0, 14.0]
            ]
        )
        o = np.tile(o, (3, 1))
        d = np.repeat(
            np.array([[1.0, 0.013, -0.03], [1.0, 0.013, 0.0], [1.0, 0.013, 0.23]]),
            len(o) // 3,
            axis=0,
        )
        d /= np.linalg.norm(d, axis=1)[:, None]
        profiles = [
            {"kind": "sun", "direction": v, "intensity": 3, "ambient": 0.1}
            for v in [[0, 0, 1], [-0.45, -0.5, 0.7], [0.45, 0.5, 0.7]]
        ]
        profiles.append(
            {
                "kind": "point",
                "position": [-50, -30, 100],
                "intensity": 3000,
                "ambient": 0.002,
            }
        )
        for p in profiles:
            rgb, coverage = np.zeros_like(o), np.zeros(len(o))
            for state in itertools.product(range(3), repeat=4):
                probability = np.prod(
                    np.array([0.15, 0.85 * 0.7, 0.85 * 0.3])[list(state)]
                )
                colour, hit = brute_image(np.array([0, 6, 12])[list(state)], o, d, p)
                rgb += probability * colour
                coverage += probability * hit
            actual, alpha, _ = render_conditional(parent, o, d, p, batch=7)
            with self.subTest(profile=p):
                np.testing.assert_allclose(alpha, coverage, rtol=0, atol=2e-14)
                np.testing.assert_allclose(actual, rgb, rtol=0, atol=2e-13)

    def test_possible_inside_mass_needs_resolved_geometry(self):
        parent = UrbanParent(
            np.ones((2, 2, 3)),
            np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
            np.array([6.0, 12.0]),
            np.array([0.7, 0.3]),
        )
        p = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3, "ambient": 0.1}
        with self.assertRaisesRegex(ValueError, "resolved product"):
            render_conditional(
                parent, np.array([[8.0, 8.0, 2.0]]), np.array([[1.0, 0.0, 0.0]]), p
            )
        # A point in the recipe's deliberate lot gap is outside every hypothesis.
        rgb, _, _ = render_conditional(
            parent, np.array([[0.5, 8.0, 2.0]]), np.array([[1.0, 0.0, 0.0]]), p
        )
        self.assertTrue(np.isfinite(rgb).all())


if __name__ == "__main__":
    unittest.main()
