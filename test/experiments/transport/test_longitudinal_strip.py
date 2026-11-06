import unittest

import numpy as np
from longitudinal_strip import correspondence_bound, parameters, simplify, triangles
from test_upper_strip_envelope import distances, samples


class LongitudinalStrip(unittest.TestCase):
    def check_correspondence(self, rings):
        bound = correspondence_bound(rings, parameters(rings))
        source, candidate = triangles(rings), triangles(rings[[0, -1]])
        slack = 256 * np.finfo(float).eps * max(1, np.abs(rings).max())
        self.assertLessEqual(distances(samples(source), candidate).max(), bound + slack)
        self.assertLessEqual(distances(samples(candidate), source).max(), bound + slack)
        return bound

    def test_straight_and_nonuniform_spacing(self):
        rings = np.array([[[x, 0, z] for x in [-3, -2, 2, 3]] for z in [0, 1, 3, 20]])
        self.assertEqual(self.check_correspondence(rings), 0)
        chosen, bound, visits = simplify(rings, 0)
        np.testing.assert_array_equal(chosen, [0, 3])
        self.assertEqual(bound, 0)
        self.assertEqual(visits, 4)

    def test_warped_strips_and_both_directions(self):
        rng = np.random.default_rng(1955)
        base = np.array([[[x, 0, z] for x in [-3, -2, 2, 3]] for z in [0, 1, 3, 5]])
        for _ in range(32):
            self.check_correspondence(base + rng.normal(size=base.shape))

    def test_source_vertices_alone_miss_the_diagonal_error(self):
        first = np.array([[x, 0, 0] for x in [-3, -1, 1, 3]], dtype=float)
        last = np.array([[x, x, 2] for x in [-3, -1, 1, 3]], dtype=float)
        rings = np.array([first, (first + last) / 2, last])
        self.assertEqual(self.check_correspondence(rings), 0.5)

    def test_identical_two_ring_topology_has_zero_error(self):
        rings = np.random.default_rng(915).normal(size=(2, 4, 3))
        self.assertEqual(correspondence_bound(rings, [0, 1]), 0)

    def test_refinement_preserves_contacts_and_budget(self):
        rings = np.array([[[x, np.sin(z), z] for x in [-3, -2, 2, 3]] for z in np.arange(9)])
        for budget in [0, 0.01, 0.5, 1, 4]:
            chosen, bound, _ = simplify(rings, budget)
            self.assertEqual(chosen[0], 0)
            self.assertEqual(chosen[-1], len(rings) - 1)
            self.assertLessEqual(bound, budget)
            for first, last in zip(chosen, chosen[1:]):
                self.assertLessEqual(
                    correspondence_bound(rings[first : last + 1], parameters(rings)[first : last + 1]),
                    budget,
                )

    def test_zero_length_centreline_still_has_a_domain(self):
        rings = np.array([[[x, 0, 0] for x in [-3, -2, 2, 3]]] * 3, dtype=float)
        np.testing.assert_array_equal(parameters(rings), [0, 1, 2])
        self.assertEqual(self.check_correspondence(rings), 0)

    def test_invalid_input(self):
        rings = np.zeros((3, 4, 3))
        for error in [-1, np.inf, np.nan]:
            with self.assertRaises(ValueError):
                simplify(rings, error)
        with self.assertRaises(ValueError):
            correspondence_bound(rings, [0, 0, 1])
        rings[0, 0, 0] = np.inf
        with self.assertRaises(ValueError):
            parameters(rings)


if __name__ == "__main__":
    unittest.main()
