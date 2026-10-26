"""Shared first-hit walks against independent worlds and repeated single draws."""

import unittest

import numpy as np
from test_urban_first_hit import small_parent, world_events
from test_urban_parent_phase import homogeneous_parent
from urban_first_hit import sample_first_hits
from urban_grouped_first_hit import sample_grouped_first_hits
from urban_parent_conditional import grid
from urban_sampled_parent import render_sampled


class GroupedFirstHitTest(unittest.TestCase):
    def compare(self, parent, o, d, u, stops=None, product=None):
        product = grid(parent) if product is None else product
        actual, work = sample_grouped_first_hits(parent, product, o, d, u, stops)
        count = u.shape[1]
        expected, direct = sample_first_hits(
            parent,
            product,
            np.repeat(o, count, axis=0),
            np.repeat(d, count, axis=0),
            u.ravel(),
            None if stops is None else np.repeat(stops, count),
        )
        for name, values in actual.items():
            np.testing.assert_array_equal(values, expected[name], err_msg=name)
        self.assertLessEqual(work["cell_ray_visits"], direct["cell_ray_visits"])
        self.assertLessEqual(
            work["height_hypothesis_tests"], direct["height_hypothesis_tests"]
        )
        return work, direct

    def test_independent_world_pmf_with_reversed_and_duplicate_draws(self):
        parent = small_parent()
        for origin, direction in [
            (np.array([-8.0, 8, 2]), np.array([1.0, 0.01, 0])),
            (np.array([40.0, 24, 8]), np.array([-1.0, -0.01, 0.01])),
            (np.array([8.0, 8, 50]), np.array([0.03, 0.1, -1.0])),
        ]:
            direction /= np.linalg.norm(direction)
            pmf = world_events(origin, direction)
            mass = np.array([p for _, p in pmf])
            mid = np.cumsum(mass) - mass / 2
            u = np.r_[mid[::-1], mid][None]
            result, _ = sample_grouped_first_hits(
                parent, grid(parent), origin[None], direction[None], u
            )
            events = [e for e, _ in pmf]
            for i, event in enumerate(events[::-1] + events):
                self.assertAlmostEqual(result["depth"][i], event[0], places=12)
                self.assertEqual(result["height_index"][i], event[1])
                np.testing.assert_array_equal(result["cell"][i], event[2:4])
                np.testing.assert_array_equal(result["normal"][i], event[4:])

    def test_nonuniform_rays_stops_and_group_sizes(self):
        parent = homogeneous_parent()
        parent.frame[:2] = [7.3, -2.6]
        parent.frame[3] = 2048
        rng = np.random.default_rng(1115)
        parent.centre_fractions = rng.uniform(0.0001, 0.02, (4, 4, 3))
        o = rng.uniform([-500, -500, 1], [2500, -40, 100], (60, 3))
        d = rng.uniform([0, 0, 0], [2048, 2048, 48], (60, 3)) - o
        distance = np.linalg.norm(d, axis=1)
        d /= distance[:, None]
        stops = np.where(rng.random(60) < 0.5, distance, np.inf)
        for count in [1, 2, 4, 16]:
            self.compare(parent, o, d, rng.random((len(o), count)), stops)
        self.compare(parent, o[:3], d[:3], rng.random((3, 512)), stops[:3])

    def test_roof_wall_cdf_ties_and_open_stops(self):
        parent = small_parent()
        o = np.array([[-8.0, 8, 8.5], [-8, 16, 8.5]])
        d = np.tile([1.0, 0, -0.25], (len(o), 1))
        u = np.tile([0.0, 0.1, 0.375, 0.6, 0.9, 0.99], (len(o), 1))
        for stop in [10.0, 26.0, np.inf]:
            self.compare(parent, o, d, u, np.full(len(o), stop))

    def test_primary_replacement_regions_are_not_sampled(self):
        parent = small_parent()
        product = grid(parent)
        product["primary_covered"] = [(np.array([0, 0]), np.array([1, 2]))]
        o, d = np.array([[-8.0, 8, 2]]), np.array([[1.0, 0, 0]])
        self.compare(parent, o, d, np.array([[0.1, 0.5, 0.9]]), product=product)

    def test_sampled_rgb_and_batch_partition_remain_exact(self):
        parent = small_parent()
        o = np.array([[-8.0, y, 2] for y in [8, 16, 24, 8]])
        d = np.tile([1.0, 0, 0], (len(o), 1))
        profiles = [
            {
                "kind": "sun",
                "direction": [-0.45, -0.5, 0.7],
                "intensity": 3.0,
                "ambient": 0.1,
            },
            {
                "kind": "point",
                "position": [-20, -30, 100],
                "intensity": 3000.0,
                "ambient": 0.002,
            },
        ]
        for profile in profiles:
            for count in [1, 4, 16]:
                expected, alpha, direct = render_sampled(
                    parent, o, d, profile, samples=count, group_first_hits=False
                )
                for batch in [1, 5]:
                    actual, beta, _ = render_sampled(
                        parent,
                        o,
                        d,
                        profile,
                        samples=count,
                        batch=batch,
                        group_first_hits=True,
                    )
                    np.testing.assert_array_equal(actual, expected)
                    np.testing.assert_array_equal(beta, alpha)
                selected, beta, work = render_sampled(
                    parent, o, d, profile, samples=count
                )
                np.testing.assert_array_equal(selected, expected)
                np.testing.assert_array_equal(beta, alpha)
                if count >= 4:
                    self.assertLess(
                        work["primary_height_hypothesis_tests"],
                        direct["primary_height_hypothesis_tests"],
                    )

    def test_empty_invalid_and_inside_eye(self):
        parent = small_parent()
        product = grid(parent)
        self.compare(parent, np.empty((0, 3)), np.empty((0, 3)), np.empty((0, 4)))
        o, d = np.array([[-8.0, 8, 2]]), np.array([[1.0, 0, 0]])
        for u in [
            np.empty((1, 0)),
            np.zeros((1, 513)),
            np.array([[np.nan]]),
            np.array([[0.1, 1.0]]),
            np.zeros(2),
        ]:
            with self.assertRaises(ValueError):
                sample_grouped_first_hits(parent, product, o, d, u)
        with self.assertRaises(ValueError):
            sample_grouped_first_hits(
                parent, product, np.array([[8.0, 8, 2]]), d, np.zeros((1, 4))
            )


if __name__ == "__main__":
    unittest.main()
