"""Authored axis continuity overrides thickness while preserving every graph edge."""

import unittest
from itertools import pairwise
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from limb_experiment import canonical_wood
from limb_paths import dominant_paths
from limb_surface import limb_mesh
from oak_skin_experiment import seed_continuations


def fork():
    points = np.array(
        [[0.0, 0, 0], [0, 0, 0.3], [0, 0, 0.6], [0.06, 0, 0.3], [0.36, 0, 0.3]]
    )
    parents = np.array([-1, 0, 1, 1, 3], np.int32)
    radii = np.array([0.2, 0.2, 0.04, 0.18, 0.18])
    return points, parents, radii


class SourceLimbTests(unittest.TestCase):
    def test_original_cubic_continuation_avoids_thickness_hairpin(self):
        points, parents, radii = fork()
        old = dominant_paths(points, parents, radii)
        self.assertEqual(old[0], (0, 1, 3, 4))
        with self.assertRaisesRegex(ValueError, "local regularity"):
            limb_mesh(points, radii, old[0])
        hints = seed_continuations(parents, parents)
        paths = dominant_paths(points, parents, radii, continuations=hints)
        self.assertEqual(paths, ((0, 1, 2), (1, 3, 4)))
        for path in paths:
            mesh = limb_mesh(points, radii, path)
            self.assertTrue(np.isfinite(mesh["normals"]).all())
        edges = np.array([e for path in paths for e in pairwise(path)])
        np.testing.assert_array_equal(np.sort(edges[:, 1]), np.arange(1, len(parents)))
        np.testing.assert_array_equal(edges[:, 0], parents[edges[:, 1]])

    def test_no_hints_preserves_default_partition_exactly(self):
        points, parents, radii = fork()
        a = dominant_paths(points, parents, radii)
        b = dominant_paths(
            points, parents, radii, continuations=np.full(len(parents), -1, np.int32)
        )
        self.assertEqual(a, b)
        original = parents.copy()
        seed_continuations(parents, parents[:3])
        np.testing.assert_array_equal(parents, original)

    def test_declared_continuations_must_be_actual_unique_directed_edges(self):
        points, parents, radii = fork()
        for hints in (
            [3, -1, -1, -1, -1],
            [5, -1, -1, -1, -1],
            [-2] * 5,
            [1.0] * 5,
            [-1],
        ):
            with self.assertRaises(ValueError):
                dominant_paths(points, parents, radii, continuations=np.array(hints))
        with self.assertRaises(ValueError):
            seed_continuations(parents, np.array([-1, 0, 0]))

    def test_whole_output_capacity_precedes_curve_work(self):
        points, parents, radii = fork()
        crown = SimpleNamespace(positions=points, parents=parents)
        with patch("limb_experiment.limb_mesh", side_effect=AssertionError("curve")):
            self.assertIsNone(canonical_wood(crown, radii=radii, max_faces=0))
            self.assertIsNone(canonical_wood(crown, radii=radii, max_nodes=3))


if __name__ == "__main__":
    unittest.main()
