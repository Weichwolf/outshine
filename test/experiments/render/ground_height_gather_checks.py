"""Check actual atlas coordinates, halo neighbours and gather component order."""

import unittest

import numpy as np


class GatherCoordinates(unittest.TestCase):
    def test_all_page_positions_and_nodes(self):
        side, columns = 35, 16
        axis = np.arange(33, dtype=np.int64)
        nodes = np.stack(np.meshgrid(axis, axis), axis=-1).reshape(-1, 2)
        scale = np.float32(1) / np.float32(side * columns)
        for page in range(512):
            offset = np.array([page % columns, page % 256 // columns]) * side
            at = offset + nodes + 1
            # GLSL gather base is floor(uv * textureSize - 0.5).
            plus = np.floor((at + 1).astype(np.float32) * scale * np.float32(560)
                            - np.float32(0.5)).astype(np.int64)
            minus = np.floor(at.astype(np.float32) * scale * np.float32(560)
                             - np.float32(0.5)).astype(np.int64)
            np.testing.assert_array_equal(plus, at)
            np.testing.assert_array_equal(minus, at - 1)
            # Gather x/y/z/w correspond to (0,1)/(1,1)/(1,0)/(0,0).
            neighbours = [plus, plus + [1, 0], minus + [0, 1],
                          minus + [1, 0], plus + [0, 1]]
            expected = [at, at + [1, 0], at - [1, 0], at - [0, 1], at + [0, 1]]
            for actual, wanted in zip(neighbours, expected, strict=True):
                np.testing.assert_array_equal(actual, wanted)
                self.assertTrue(np.all(actual >= offset))
                self.assertTrue(np.all(actual < offset + side))


if __name__ == "__main__":
    unittest.main()
