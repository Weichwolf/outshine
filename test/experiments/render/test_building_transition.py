"""A complete-layer transition cannot make a shared opaque surface transparent."""

import unittest

import numpy as np
from build_building_transition import transition_queries
from building_source_tree import build_tree, select_cut
from building_transition import independent_fade_over, mix_layers


class BuildingTransition(unittest.TestCase):
    def test_shared_opaque_surface_does_not_leak_bright_background(self):
        first = {"C": np.array([[[1, 0, 0.0]]]), "T": np.zeros((1, 1, 1))}
        second = {"C": np.array([[[0, 0, 1.0]]]), "T": np.zeros((1, 1, 1))}
        mixed = mix_layers(first, second, 0.5)
        np.testing.assert_array_equal(mixed["C"], [[[0.5, 0, 0.5]]])
        np.testing.assert_array_equal(mixed["T"], 0)
        naive = independent_fade_over(first, second, 0.5)
        np.testing.assert_array_equal(naive["T"], 0.25)
        for background in (0, 0.1, 10):
            expected = 0.5 * (first["C"] + background * first["T"]) + 0.5 * (
                second["C"] + background * second["T"]
            )
            np.testing.assert_array_equal(
                mixed["C"] + background * mixed["T"], expected
            )
        for weight, endpoint in ((0, first), (1, second)):
            for key, value in mix_layers(first, second, weight).items():
                np.testing.assert_array_equal(value, endpoint[key])
        with self.assertRaises(ValueError):
            mix_layers(first, second, 1.01)

    def test_exact_root_boundary_changes_cut_but_not_render_eye(self):
        tree = build_tree([1, 2], [[[0, 0, 0], [1, 1, 1]], [[2, 0, 0], [3, 1, 1]]])
        probes = transition_queries(tree)
        for finer, coarser in (probes[:2], probes[2:]):
            np.testing.assert_array_equal(finer["render_eye"], coarser["render_eye"])
            fine, _ = select_cut(tree, finer["eye"], finer["focal_px"])
            coarse, _ = select_cut(tree, coarser["eye"], coarser["focal_px"])
            self.assertGreater(len(fine), 1)
            np.testing.assert_array_equal(coarse, [0])


if __name__ == "__main__":
    unittest.main()
