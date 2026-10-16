"""Complete edge partition, canonical anchors, closed-ring topology and demanded work."""

import unittest
from itertools import pairwise
from unittest.mock import patch

import numpy as np
from colonization import pipe_radii
from limb_paths import dominant_paths
from limb_surface import limb_mesh, path_recipe
from scipy.spatial.transform import Rotation


class LimbChecks(unittest.TestCase):
    def test_dominance_every_edge_once_and_numeric_direction_id_ties(self):
        points = np.array(
            ((0, 0, 0), (0, 0, 1), (0, 0, 2), (1, 0, 2), (0, 0, 3), (1, 0, 3)), float
        )
        parents = np.array((-1, 0, 1, 1, 2, 2))
        radii = pipe_radii(parents, 0.1)
        paths = dominant_paths(points, parents, radii)
        self.assertEqual(paths, ((0, 1, 2, 4), (1, 3), (2, 5)))
        edges = np.array(
            [(a, b) for path in paths for a, b in pairwise(path)]
        )
        np.testing.assert_array_equal(np.sort(edges[:, 1]), np.arange(1, len(points)))
        np.testing.assert_array_equal(edges[:, 0], parents[edges[:, 1]])
        transformed = Rotation.from_rotvec((0.3, -0.7, 0.2)).apply(points) * 7 + (
            5,
            3,
            -1,
        )
        self.assertEqual(dominant_paths(transformed, parents, radii * 7), paths)

    def test_straight_frustum_independent_polygon_volume_and_only_terminal_caps(self):
        points = np.array(((0, 0, 0), (0, 0, 1)), float)
        radii = np.array((0.1, 0.1))
        for along in (1, 2, 8):
            mesh = limb_mesh(points, radii, (0, 1), along, 8)
            p = mesh["vertices"][mesh["faces"]]
            volume = (
                np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
            )
            polygon_area = 8 * np.sin(2 * np.pi / 8) / 2
            expected = polygon_area / 3 * (0.1**2 + 0.1 * 0.035 + 0.035**2)
            self.assertAlmostEqual(volume, expected, places=14)
            edges = np.sort(
                np.concatenate([mesh["faces"][:, i] for i in ([0, 1], [1, 2], [2, 0])]),
                axis=1,
            )
            _, count = np.unique(edges, axis=0, return_counts=True)
            self.assertTrue(np.all(count == 2))
            self.assertEqual(len(mesh["faces"]), 16 * (along + 1))

    def test_source_nodes_are_exact_curve_anchors_and_radius_profile_is_monotone(self):
        points = np.array(
            ((0, 0, 0), (0.1, 0, 0.4), (0.3, 0.1, 0.8), (0.6, 0.2, 1.2)), float
        )
        radii = np.array((0.04, 0.035, 0.02, 0.01))
        spine, profile = path_recipe(points, radii, (0, 1, 2, 3))
        np.testing.assert_allclose(
            spine.evaluate(np.linspace(0, 1, 4)), points, atol=1e-14
        )
        values = profile(np.linspace(0, 1, 513))
        self.assertTrue(np.all(values > 0))
        self.assertTrue(np.all(np.diff(values) <= 1e-14))
        mesh = limb_mesh(points, radii, (0, 1, 2, 3), 4, 12)
        n = mesh["normals"]
        np.testing.assert_allclose(np.linalg.norm(n, axis=1), 1, atol=1e-14)
        # Each shared interior ring vertex has one side normal on all incident faces.
        faces = mesh["faces"][:-24]
        normals = n.reshape(-1, 3, 3)[:-24]
        for vertex in np.unique(faces):
            seen = normals[np.where(faces == vertex)]
            np.testing.assert_array_equal(seen, np.broadcast_to(seen[0], seen.shape))

    def test_output_capacity_precedes_curve_frames_or_mapping(self):
        points = np.array(((0, 0, 0), (0, 0, 1)), float)
        radii = np.array((0.1, 0.1))
        with patch(
            "limb_surface.path_recipe", side_effect=AssertionError("unbudgeted curve")
        ):
            self.assertIsNone(limb_mesh(points, radii, (0, 1), max_faces=0))
            self.assertIsNone(limb_mesh(points, radii, (0, 1), max_vertices=0))
        with self.assertRaises(ValueError):
            dominant_paths(points, np.array((-1, 1)), radii)
        self.assertIsNone(
            dominant_paths(
                np.vstack((points, (0, 0, 2))),
                np.array((-1, 0, 1)),
                np.full(3, 0.1),
                max_nodes=2,
            )
        )


if __name__ == "__main__":
    unittest.main()
