import unittest

import numpy as np
from outshine.rendering.irradiance_mesh import refine_triangles


class IrradianceMeshTest(unittest.TestCase):
    def test_refinement_preserves_sloped_triangles_and_shared_edges(self):
        points = np.array(
            [
                (0, 0, 0),
                (64, 0, 0),
                (64, 0.1, 0.03),
                (0, -2, -0.6),
                (100, 100, 30),
                (120, 100, 30),
                (100, 104, 31.2),
            ]
        )
        faces = np.array([(0, 1, 2), (1, 0, 3), (4, 5, 6)], dtype=np.int32)
        eye = np.array([12, 0, 1.7])
        vertices, triangles, field = refine_triangles(
            points, faces, eye, 38, 2, points[:, 0] + 2 * points[:, 1]
        )
        np.testing.assert_allclose(vertices[:, 2], 0.3 * vertices[:, 1], atol=1e-10)
        np.testing.assert_allclose(field, vertices[:, 0] + 2 * vertices[:, 1])
        self.assertTrue(np.any(np.all(triangles == faces[-1], axis=1)))

        corners = vertices[triangles]
        area = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
        self.assertTrue(np.all(area[:, 2] > 0))
        self.assertAlmostEqual(area[:, 2].sum(), 64 * 2.1 + 20 * 4)
        centres = corners.mean(axis=1)
        radius = np.linalg.norm(corners - centres[:, None, :], axis=2).max(axis=1)
        nearby = np.linalg.norm(centres - eye, axis=1) < 38 + radius
        edge_lengths = np.linalg.norm(corners - np.roll(corners, -1, axis=1), axis=2)
        self.assertLessEqual(edge_lengths[nearby].max(), 2)

        edges = np.stack((triangles, np.roll(triangles, -1, axis=1)), axis=2)
        shared = edges[(vertices[edges, 1] == 0).all(axis=2)]
        _, incidence = np.unique(np.sort(shared), axis=0, return_counts=True)
        self.assertGreater(len(incidence), 3)
        self.assertTrue(np.all(incidence == 2))
