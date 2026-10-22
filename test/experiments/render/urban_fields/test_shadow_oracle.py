"""Check the all-prism diagnostic against independently indexed triangles."""

import unittest

import numpy as np
from shadow_oracle import bounds, nearest
from triangle_image_reference import box_grid_source, intersect_source


class ShadowOracleTest(unittest.TestCase):
    def test_boxes_floor_parallel_rays_and_empty_grid(self):
        rng = np.random.default_rng(1127)
        heights = rng.choice([0.0, 2.0, 4.0, 8.0], (3, 4))
        origins = rng.uniform([-8, 12, -8], [24, 20, 20], (96, 3))
        targets = rng.uniform([0, 0, 0], [16, 0, 12], (96, 3))
        origins = np.vstack(
            (origins, [[-5, 1, 2], [2, 1, -5], [2, 12, 2], [-5, 1, -2]])
        )
        directions = np.vstack(
            (targets - origins[:96], [[1, 0, 0], [0, 0, 1], [0, -1, 0], [1, 0, 0]])
        )
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        for grid in (heights, np.zeros_like(heights)):
            source = box_grid_source(grid, 4.0, 0.5)
            source["positions"] = source["positions"][:, [0, 2, 1]]
            source["indices"] = source["indices"][:, [0, 2, 1]]
            count = len(source["positions"])
            source["positions"] = np.vstack(
                (source["positions"], [[0, 0, 0], [16, 0, 0], [16, 0, 12], [0, 0, 12]])
            )
            source["indices"] = np.vstack(
                (source["indices"], np.array([[0, 2, 1], [0, 3, 2]]) + count)
            )
            expected, _ = intersect_source(source, origins, directions)
            for native in (True, False):
                low, high, _ = bounds(grid, 4.0, 0.5, native)
                actual = []
                for origin, direction in zip(origins, directions, strict=True):
                    result = nearest(origin, direction, low, high, np.array([16, 12]))
                    actual.append(
                        np.inf
                        if result is None
                        else np.dot(result[0] - origin, direction)
                    )
                np.testing.assert_allclose(actual, expected, rtol=1e-10, atol=1e-10)


if __name__ == "__main__":
    unittest.main()
