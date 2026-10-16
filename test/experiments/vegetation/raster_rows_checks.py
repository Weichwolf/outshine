"""Exact fragment/reference comparisons including thin and ill-conditioned rows."""

import unittest

import numpy as np
from grass_projection import contacts
from raster_batch import fragments as boxes
from raster_rows import fragments as rows


def encoded(method, vertices, faces, size, budget):
    pieces = list(method(vertices, faces, (-1, -1), (1, 1), size, budget))
    if not pieces:
        return np.empty((0, 6))
    triangle, weights, pixel, z = (
        np.concatenate([p[i] for p in pieces]) for i in range(4)
    )
    data = np.c_[triangle, pixel, weights, z]
    return data[np.lexsort((pixel, triangle))]


class RasterRowsChecks(unittest.TestCase):
    def test_random_and_grid_boundary_fragments(self):
        rng = np.random.Generator(np.random.PCG64(465))
        cases = [rng.uniform(-3, 3, (90, 3)), rng.uniform(-0.2, 0.2, (90, 3))]
        cases.append(
            np.array(
                (
                    (-1, -1, 0),
                    (1, 1, 0.7),
                    (1, -1, -0.3),
                    (-0.5, -0.5, 0.2),
                    (0.5, -0.5, 0.3),
                    (0, 0.5, 0.4),
                )
            )
        )
        for vertices in cases:
            for size in (16, 64):
                faces = np.arange(len(vertices)).reshape(-1, 3)
                for face_order in (faces, faces[:, ::-1]):
                    expected = encoded(boxes, vertices, face_order, size, size * size)
                    for budget in (size * size, 262144):
                        np.testing.assert_array_equal(
                            encoded(rows, vertices, face_order, size, budget), expected
                        )

    def test_long_thin_and_near_degenerate_geometry(self):
        triangles = []
        for width in (1e-15, 1e-10, 1e-5, 0.01):
            for direction in ((1, 0), (0, 1), (1, 1), (1, -0.2)):
                d = np.array((*direction, 0.1), float)
                n = np.array((-direction[1], direction[0], 0))
                triangles.extend(
                    [
                        (-0.9 * d, 0.9 * d, 0.9 * d + width * n),
                        (
                            (0.03125, 0.03125, 0),
                            (0.03125, 0.03125, 1),
                            (0.03125 + width, 0.9, 0.2),
                        ),
                    ]
                )
        vertices = np.array(triangles).reshape(-1, 3)
        faces = np.arange(len(vertices)).reshape(-1, 3)
        expected = encoded(boxes, vertices, faces, 64, 4096)
        np.testing.assert_array_equal(
            encoded(rows, vertices, faces, 64, 4096), expected
        )
        work = {}
        list(rows(vertices, faces, (-1, -1), (1, 1), 64, 4096, work))
        self.assertGreater(work["unpruned_rows"], 0)
        self.assertLessEqual(work["max_pair_batch"], 4096)
        self.assertLessEqual(work["max_row_batch"], 4096)

    def test_contact_and_canonical_depth_ties(self):
        v = np.array(((-1, -1, 0.2), (1, 1, 0.2), (1, -1, 0.2)))
        vertices, faces = np.r_[v, v], np.arange(6).reshape(-1, 3)
        for budget in (1024, 4096):
            a = contacts(
                vertices,
                faces,
                (-1, -1),
                (1, 1),
                (0, 0, 1),
                32,
                budget,
                sampler="boxes",
            )
            b = contacts(
                vertices, faces, (-1, -1), (1, 1), (0, 0, 1), 32, budget, sampler="rows"
            )
            for key in ("face", "height", "normal", "coverage"):
                np.testing.assert_array_equal(a[key], b[key])

    def test_empty_offscreen_and_invalid_budget(self):
        v = np.array(((3, 3, 0), (4, 3, 0), (3, 4, 0)))
        f = np.array(((0, 1, 2),))
        self.assertEqual(len(encoded(rows, v, f, 16, 256)), 0)
        self.assertEqual(
            len(encoded(rows, np.empty((0, 3)), np.empty((0, 3), int), 16, 256)), 0
        )
        with self.assertRaises(ValueError):
            list(rows(v, f, (-1, -1), (1, 1), 16, 1))


if __name__ == "__main__":
    unittest.main()
