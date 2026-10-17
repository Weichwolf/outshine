"""Check installed meshoptimizer cone rejection against every source triangle plane."""

import argparse
import unittest
from pathlib import Path

import numpy as np
from terrain_cones import cluster_bounds, reject
from terrain_grid_lod import triangles

LIBRARY = None


class TerrainCones(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bounds = staticmethod(cluster_bounds(LIBRARY))

    def test_planar_surface_keeps_above_and_rejects_below(self):
        y, x = np.indices((9, 9), dtype=float)
        points = np.stack((x, -y, 3 * x - 2 * y), axis=-1).reshape(-1, 3)
        faces = triangles(9, 1)
        self.assertFalse(
            reject(self.bounds, points, faces, np.array([4.0, -4.0, 10.0]), 1e-4)
        )
        self.assertTrue(
            reject(self.bounds, points, faces, np.array([4.0, -4.0, -10.0]), 1e-4)
        )

    def test_reversed_winding_swaps_the_visible_side(self):
        y, x = np.indices((9, 9), dtype=float)
        points = np.stack((x, y, np.zeros_like(x)), axis=-1).reshape(-1, 3)
        faces = triangles(9, 1)
        self.assertTrue(
            reject(self.bounds, points, faces, np.array([4.0, 4.0, 10.0]), 1e-4)
        )
        self.assertFalse(
            reject(self.bounds, points, faces, np.array([4.0, 4.0, -10.0]), 1e-4)
        )

    def test_every_rejected_random_cluster_has_no_frontfacing_triangle(self):
        rng = np.random.default_rng(515)
        faces = triangles(9, 1)
        y, x = np.indices((9, 9), dtype=float)
        rejected = 0
        for _ in range(30):
            h = 0.2 * rng.normal(size=(9, 9)) + 0.3 * x
            points = (
                np.stack((x, -y, h), axis=-1)
                .reshape(-1, 3)
                .astype("float32")
                .astype(float)
            )
            p = points[faces]
            normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
            for eye in rng.normal(size=(40, 3)) * 30:
                if reject(self.bounds, points, faces, eye, 1e-4):
                    rejected += 1
                    self.assertTrue(
                        np.all(np.einsum("ni,ni->n", normals, eye - p[:, 0]) <= 0)
                    )
        self.assertGreater(rejected, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path)
    args, extra = parser.parse_known_args()
    LIBRARY = args.library
    unittest.main(argv=["verify_terrain_cones.py", *extra])
