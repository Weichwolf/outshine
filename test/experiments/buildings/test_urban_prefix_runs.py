"""Affine primary prefixes against independent boxes and original DDA contacts."""

import unittest

import numpy as np
from test_urban_parent_phase import homogeneous_parent
from test_urban_parent_prefix import enumerated_prefix
from urban_parent_prefix import dda_prefix
from urban_prefix_runs import PrefixRuns


class PrefixRunsTest(unittest.TestCase):
    def test_nonuniform_prefixes_match_individually_intersected_boxes(self):
        rng = np.random.default_rng(1113)
        parent = homogeneous_parent()
        parent.frame[:2] = [7.3, -2.6]
        parent.centre_fractions = rng.uniform(0.0001, 0.05, (4, 4, 3))
        origins = rng.uniform([-512, -512, 1], [1024, 1024, 160], (180, 3))
        targets = rng.uniform([-64, -64, 0], [640, 640, 80], (180, 3))
        directions = targets - origins
        distances = np.linalg.norm(directions, axis=1)
        directions /= distances[:, None]
        stops = np.where(rng.random(180) < 0.5, distances, np.inf)
        actual, work = PrefixRuns(parent, force=True).transmission(
            origins, directions, stops
        )
        expected = np.array(
            [
                enumerated_prefix(parent, o, d, t)
                for o, d, t in zip(origins, directions, stops, strict=True)
            ]
        )
        np.testing.assert_allclose(actual, expected, rtol=3e-11, atol=3e-13)
        direct, _ = dda_prefix(parent, origins, directions, stops)
        np.testing.assert_allclose(actual, direct, rtol=3e-11, atol=3e-13)
        self.assertGreater(work["affine_runs"], 0)

    def test_tangent_roof_corner_and_open_prefix_contacts_match_dda(self):
        parent = homogeneous_parent()
        parent.frame[3] = 128
        model = PrefixRuns(parent, force=True)
        h = parent.heights[0]
        edge = 8 - parent.frame[5] * 8
        origins = np.array(
            [
                [8, -16, h],
                [8, -16, parent.heights[-1]],
                [edge, -16, h],
                [16 - edge, -16, h],
                [-16, -16, h],
                [8, -16, h + 8],
                [8, -16, h / 2],
                [15.68, -16, 2],
            ],
            float,
        )
        directions = np.array(
            [
                [0, 1, 0],
                [0, 1, 0],
                [0, 1, 0],
                [0, 1, 0],
                [1, 1, 0],
                [0, 1, -1],
                [0, 1, 1],
                [0, 1, 0],
            ],
            float,
        )
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        for stop in [16 + edge, 24, 128, np.inf]:
            stops = np.full(len(origins), stop)
            actual, _ = model.transmission(origins, directions, stops)
            expected, _ = dda_prefix(parent, origins, directions, stops)
            np.testing.assert_allclose(actual, expected, rtol=3e-12, atol=2e-14)
            self.assertEqual(actual[-1], 1)

    def test_long_low_density_runs_and_reversed_directions(self):
        parent = homogeneous_parent()
        parent.frame[3] = 8192
        parent.centre_fractions = np.linspace(0.00001, 0.0001, 4)[
            :, None, None
        ] * np.ones((4, 4, 3))
        origins = np.array(
            [[8, -32, 2], [8, 8224, 2], [15.68, -32, 2], [4096, -32, 2]], float
        )
        directions = np.array([[0, 1, 0], [0, -1, 0], [0, 1, 0], [0, 1, 0]], float)
        actual, work = PrefixRuns(parent, force=True).transmission(origins, directions)
        expected, _ = dda_prefix(parent, origins, directions)
        np.testing.assert_allclose(actual, expected, rtol=3e-12, atol=3e-13)
        self.assertEqual(actual[2], 1)
        self.assertGreater(work["gamma_cells"], 16)
        self.assertLess(work["run_source_endpoint_samples"], work["affine_cells"])

    def test_dispatch_budget_empty_and_invalid(self):
        parent = homogeneous_parent()
        origins = np.array([[8, -32, 2], [8, -32, 200], [15.68, -32, 2]], float)
        directions = np.array([[0, 1, 0], [0, 0, 1], [0, 1, 0]], float)
        for force, budget in [(False, 131072), (True, 0)]:
            actual, work = PrefixRuns(
                parent, force=force, column_budget=budget
            ).transmission(origins, directions)
            expected, _ = dda_prefix(parent, origins, directions)
            np.testing.assert_allclose(actual, expected, rtol=3e-12, atol=2e-14)
            self.assertEqual(work["clear_receivers"], 2)
        model = PrefixRuns(parent)
        actual, _ = model.transmission(np.empty((0, 3)), np.empty((0, 3)))
        self.assertEqual(len(actual), 0)
        for o, d, t in [
            ([[0, 0, -1]], [[0, 1, 0]], None),
            ([[0, 0, 1]], [[0, 0, 0]], None),
            ([[0, 0, 1]], [[0, 1, 0]], [np.nan]),
        ]:
            with self.assertRaises(ValueError):
                model.transmission(o, d, t)

    def test_column_budget_is_shared_by_the_packet(self):
        parent = homogeneous_parent()
        origins = np.tile([8, -32, 2.0], (30, 1))
        directions = np.tile([0, 1.0, 0], (30, 1))
        value, work = PrefixRuns(parent, force=True, column_budget=20).transmission(
            origins, directions
        )
        expected, _ = dda_prefix(parent, origins, directions)
        np.testing.assert_allclose(value, expected, atol=2e-14, rtol=3e-12)
        self.assertGreater(work["run_receivers"], 0)
        self.assertGreater(work["dda_receivers"], 0)
        self.assertLessEqual(work["run_columns"], 20 * len(parent.heights))


if __name__ == "__main__":
    unittest.main()
