"""Geometric counterexamples for the point-light experiment, independent of radiance."""

import unittest

import numpy as np
from local_light_grid import build_grid
from local_light_inputs import Camera, Lights


class LocalLightGrid(unittest.TestCase):
    def test_support_at_tile_depth_and_camera_boundaries(self):
        random = np.random.default_rng(448)
        for perspective in (True, False):
            for angle in (0.0, 0.7):
                cosine, sine = np.cos(angle), np.sin(angle)
                basis = np.array([[cosine, 0, sine], [0, 1, 0], [-sine, 0, cosine]])
                frame = Camera(
                    np.array([123.25, -57.5, 201.0]),
                    basis,
                    perspective,
                    np.array([16 / 9, 1.0]) * (0.6 if perspective else 1200),
                )
                xy = random.uniform(-0.999, 0.999, (240, 2))
                # Deliberately include projected tile boundaries, not only random interiors.
                xy[:30, 0] = (np.arange(30) * 64 / frame.width) * 2 - 1
                z = np.r_[np.geomspace(0.1, 4096, 24), np.full(216, 5000)]
                scale = z if perspective else np.ones_like(z)
                view = np.c_[xy * frame.half * scale[:, None], z]
                points = view @ basis + frame.eye
                directions = random.normal(size=points.shape)
                directions /= np.linalg.norm(directions, axis=1)[:, None]
                radii = random.uniform(1, 64, len(points))
                positions = points + directions * radii[:, None] * 0.99
                # Behind-eye and unbounded lights exercise the near-plane/global path.
                positions = np.vstack((positions, frame.eye - basis[2], frame.eye))
                radii = np.r_[radii, [32, 0]]
                lights = Lights(positions, radii, np.ones_like(positions), [])
                distance = np.linalg.norm(points[:, None] - positions, axis=2)
                required = (radii == 0) | ((distance < radii) & (distance > 0))
                for mode in ("tiles", "linear", "log"):
                    with self.subTest(perspective=perspective, angle=angle, mode=mode):
                        grid = build_grid(lights, frame, mode)
                        self.assertFalse(grid.stats["overflow_full_list"])
                        for row, key in enumerate(grid.keys(points)):
                            first, count = map(int, grid.headers[key])
                            ids = np.r_[
                                grid.globals, grid.indices[first : first + count]
                            ]
                            self.assertEqual(len(ids), len(np.unique(ids)))
                            self.assertTrue(
                                set(np.flatnonzero(required[row])) <= set(ids)
                            )

    def test_overflow_preserves_every_emitter_and_empty_scene(self):
        frame = Camera(np.zeros(3), np.eye(3), True, np.array([16 / 9, 1]))
        lights = Lights(
            np.array([[0, 0, 1], [0, 0, 10], [0, 0, 100.0]]),
            np.array([32, 32, 0.0]),
            np.ones((3, 3)),
            [],
        )
        grid = build_grid(lights, frame, index_budget=4)
        self.assertTrue(grid.stats["overflow_full_list"])
        np.testing.assert_array_equal(grid.globals, np.arange(3))
        self.assertEqual(grid.indices.size, 0)
        self.assertEqual(grid.headers.sum(), 0)
        empty = Lights(np.empty((0, 3)), np.empty(0), np.empty((0, 3)), [])
        grid = build_grid(empty, frame)
        self.assertFalse(grid.stats["overflow_full_list"])
        self.assertEqual(grid.indices.size + grid.globals.size, 0)
        self.assertEqual(grid.headers.sum(), 0)


if __name__ == "__main__":
    unittest.main()
