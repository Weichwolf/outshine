"""Check shared-cell gather values against independent scalar atlas fetches."""

import unittest

import numpy as np


class TriangleSamples(unittest.TestCase):
    def test_every_cell_and_atlas_slot(self):
        side, columns, layers = 35, 16, 2
        dimension = side * columns
        atlas = (
            np.random.default_rng(1182)
            .uniform(-9000, 9000, (layers, dimension, dimension))
            .astype(np.float32)
        )
        cells = np.stack(np.meshgrid(np.arange(32), np.arange(32)), axis=-1).reshape(
            -1, 2
        )
        scale = np.float32(1) / np.float32(dimension)

        def read(layer, xy):
            return atlas[layer, xy[:, 1], xy[:, 0]]

        for page in range(512):
            offset = np.array([page % columns, page % 256 // columns]) * side
            centre = offset + cells + 1
            layer = page // 256
            gathers = []
            for delta in ([0, 0], [2, 0], [0, 2], [2, 2]):
                uv = (centre + delta).astype(np.float32) * scale
                base = np.floor(uv * np.float32(dimension) - np.float32(0.5)).astype(
                    np.int64
                )
                np.testing.assert_array_equal(base, centre + delta - 1)
                taps = np.stack(
                    [base + d for d in ([0, 1], [1, 1], [1, 0], [0, 0])], axis=1
                )
                self.assertTrue(np.all(taps >= offset))
                self.assertTrue(np.all(taps < offset + side))
                gathers.append(
                    np.stack([read(layer, taps[:, i]) for i in range(4)], axis=1)
                )
            nw, ne, sw, se = [g.T for g in gathers]
            heights = np.stack((nw[1], ne[0], se[3], sw[2]), axis=1)
            east = np.stack(
                (ne[0] - nw[0], ne[1] - nw[1], se[2] - sw[2], se[3] - sw[3]), axis=1
            )
            north = np.stack(
                (nw[2] - sw[2], ne[3] - se[3], ne[0] - se[0], nw[1] - sw[1]), axis=1
            )
            for i, delta in enumerate(([0, 0], [1, 0], [1, 1], [0, 1])):
                at = centre + delta
                expected_east = read(layer, at + [1, 0]) - read(layer, at - [1, 0])
                expected_north = read(layer, at - [0, 1]) - read(layer, at + [0, 1])
                np.testing.assert_array_equal(heights[:, i], read(layer, at))
                np.testing.assert_array_equal(east[:, i], expected_east)
                np.testing.assert_array_equal(north[:, i], expected_north)


if __name__ == "__main__":
    unittest.main()
