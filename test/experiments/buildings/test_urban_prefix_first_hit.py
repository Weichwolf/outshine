"""Prefix inversion versus independent box-world PMFs and complete event walking."""

import unittest

import numpy as np
from test_urban_first_hit import small_parent, world_events
from test_urban_parent_phase import homogeneous_parent
from urban_first_hit import sample_first_hits
from urban_prefix_first_hit import PrefixFirstHits
from urban_prefix_runs import PrefixRuns
from urban_sampled_parent import render_sampled


class PrefixFirstHitTest(unittest.TestCase):
    def compare(self, parent, origins, directions, uniform, stops=None, *, force=True):
        model = PrefixRuns(parent)
        actual, work = PrefixFirstHits(model, force=force).sample(
            origins, directions, uniform, stops
        )
        expected, _ = sample_first_hits(
            parent, model.product, origins, directions, uniform, stops
        )
        for key, value in actual.items():
            np.testing.assert_array_equal(value, expected[key], err_msg=key)
        return work

    def test_forced_random_angles_heights_misses_and_open_stops(self):
        parent = homogeneous_parent()
        parent.frame[:2] = [7.3, -2.6]
        parent.frame[3] = 2048
        rng = np.random.default_rng(1135)
        parent.centre_fractions = rng.uniform(0.0001, 0.02, (4, 4, 3))
        origins = rng.uniform([-500, -500, 1], [2500, -40, 100], (120, 3))
        targets = rng.uniform([0, 0, 0], [2048, 2048, 48], (120, 3))
        directions = targets - origins
        distance = np.linalg.norm(directions, axis=1)
        directions /= distance[:, None]
        stops = np.where(rng.random(120) < 0.5, distance, np.inf)
        self.compare(parent, origins, directions, rng.random(120), stops)

    def test_rgb_and_batch_partition_preserve_same_surface_draws(self):
        parent = homogeneous_parent()
        parent.frame[3] = 8192
        parent.centre_fractions *= 1e-4
        o = np.array([[8.0, -32, 2], [24, -32, 8]])
        d = np.tile([0.0, 1, 0], (2, 1))
        profile = {
            "kind": "sun",
            "direction": [-0.45, -0.5, 0.7],
            "intensity": 3.0,
            "ambient": 0.1,
        }
        expected, alpha, _ = render_sampled(parent, o, d, profile, samples=4)
        for batch in [1, 5]:
            actual, beta, work = render_sampled(
                parent, o, d, profile, samples=4, batch=batch, first_hit_prefix=True
            )
            np.testing.assert_array_equal(actual, expected)
            np.testing.assert_array_equal(beta, alpha)
            self.assertGreater(work["primary_inverted_receivers"], 0)

    def test_independently_enumerated_first_event_pmfs(self):
        parent = small_parent()
        for origin, direction in [
            (np.array([-8.0, 8, 2]), np.array([1.0, 0.01, 0])),
            (np.array([40.0, 24, 8]), np.array([-1.0, -0.01, 0.01])),
            (np.array([8.0, 8, 50]), np.array([0.03, 0.1, -1.0])),
        ]:
            direction /= np.linalg.norm(direction)
            pmf = world_events(origin, direction)
            mass = np.array([p for _, p in pmf])
            uniform = np.cumsum(mass) - mass / 2
            origins = np.tile(origin, (len(pmf), 1))
            directions = np.tile(direction, (len(pmf), 1))
            result, _ = PrefixFirstHits(PrefixRuns(parent), force=True).sample(
                origins, directions, uniform
            )
            for i, (event, _) in enumerate(pmf):
                self.assertAlmostEqual(result["depth"][i], event[0], places=12)
                self.assertEqual(result["height_index"][i], event[1])
                np.testing.assert_array_equal(result["cell"][i], event[2:4])
                np.testing.assert_array_equal(result["normal"][i], event[4:])

    def test_sparse_nonuniform_long_rays_reversed_and_finite_stops(self):
        parent = homogeneous_parent()
        parent.frame[:2] = [7.3, -2.6]
        parent.frame[3] = 8192
        rng = np.random.default_rng(1134)
        parent.centre_fractions = rng.uniform(0.00001, 0.0001, (4, 4, 3))
        for reverse in [False, True]:
            origins = np.array(
                [[x, -32, z] for x in [8, 70, 125] for z in [2, 8, 20]], float
            )
            directions = np.tile([0.003, 1.0, -0.0001], (len(origins), 1))
            if reverse:
                origins[:, 1] = 8224
                directions[:, :2] *= -1
            stops = np.linspace(1000, 9000, len(origins))
            model = PrefixRuns(parent)
            transmission, _ = model.transmission(origins, directions, stops)
            uniform = (1 - transmission) * np.linspace(0.1, 0.9, len(origins))
            work = self.compare(parent, origins, directions, uniform, stops)
            self.assertGreater(work["inverted_receivers"], 0)
            self.assertLessEqual(work.get("tail_cell_ray_visits", 0), 3 * len(origins))

    def test_contacts_and_cdf_boundaries_keep_original_labels(self):
        parent = small_parent()
        o = np.tile([-8.0, 8, 8.5], (6, 1))
        d = np.tile([1.0, 0, -0.25], (6, 1))
        u = np.array([0.0, 0.1, 0.375, 0.6, 0.9, 0.99])
        self.compare(parent, o, d, u)
        self.compare(parent, o, d, u, np.full(6, 10.0))
        parent = homogeneous_parent()
        parent.centre_fractions[:] = 0.0001
        model = PrefixRuns(parent)
        o, d = np.array([[8.0, -32, 2]]), np.array([[0.0, 1, 0]])
        t, _ = model.transmission(o, d)
        work = self.compare(parent, o, d, 1 - t)
        self.assertEqual(work["ambiguous_receivers"], 1)

    def test_corridor_short_diagonal_vertical_empty_and_validation(self):
        parent = homogeneous_parent()
        origins = np.array(
            [[15.68, -32, 2], [8, -32, 2], [-32, -32, 8], [8, 8, 100]], float
        )
        directions = np.array([[0, 1, 0], [0, 1, 0], [1, 1, 0], [0, 0, -1]], float)
        work = self.compare(
            parent, origins, directions, np.array([0.1, 0.2, 0.5, 0.9]), force=False
        )
        self.assertGreater(work["fallback_receivers"], 0)
        self.compare(parent, np.empty((0, 3)), np.empty((0, 3)), np.empty(0))
        query = PrefixFirstHits(PrefixRuns(parent))
        for u in [np.array([-0.1]), np.array([1.0]), np.array([np.nan])]:
            with self.assertRaises(ValueError):
                query.sample(origins[:1], directions[:1], u)
        with self.assertRaises(ValueError):
            query.sample(np.array([[8.0, 8, 2]]), directions[:1], np.array([0.5]))


if __name__ == "__main__":
    unittest.main()
