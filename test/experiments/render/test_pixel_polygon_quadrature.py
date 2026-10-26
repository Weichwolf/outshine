"""Independent GEOS intersection and triangle-moment controls."""

import unittest

import numpy as np
from pixel_polygon_quadrature import pixel_quadrature
from shapely.geometry import MultiPoint, Point, box


def moments(polygon):
    vertices = np.array(polygon.exterior.coords[:-1])
    result = np.zeros(6)
    for i in range(1, len(vertices) - 1):
        triangle = vertices[[0, i, i + 1]]
        a, b = triangle[1:] - triangle[0]
        area = abs(a[0] * b[1] - a[1] * b[0]) / 2
        x, y = triangle.T
        result += area * np.array(
            [
                1,
                x.sum() / 3,
                y.sum() / 3,
                (x.sum() ** 2 + x @ x) / 12,
                (y.sum() ** 2 + y @ y) / 12,
                (x.sum() * y.sum() + x @ y) / 12,
            ]
        )
    return result


class PixelQuadratureTest(unittest.TestCase):
    def test_random_intersections_and_quadratic_moments(self):
        rng = np.random.default_rng(1113)
        bounds = np.array([[-2, -1], [3, 3]])
        for _ in range(35):
            hull = MultiPoint(rng.uniform([-3, -2], [4, 4], (10, 2))).convex_hull
            polygon = np.array(hull.exterior.coords[:-1])
            xy, ids, weights, work = pixel_quadrature(polygon, bounds, order=2)
            self.assertTrue(np.all(weights > 0))
            self.assertEqual(work["quadrature_samples"], len(xy))
            for owner in range(20):
                x, y = owner % 5 - 2, owner // 5 - 1
                intersection = hull.intersection(box(x, y, x + 1, y + 1))
                at = ids == owner
                self.assertAlmostEqual(weights[at].sum(), intersection.area, places=12)
                if intersection.area == 0:
                    continue
                sample_x, sample_y = xy[at].T
                features = np.array(
                    [
                        np.ones(at.sum()),
                        sample_x,
                        sample_y,
                        sample_x**2,
                        sample_y**2,
                        sample_x * sample_y,
                    ]
                )
                np.testing.assert_allclose(
                    features @ weights[at], moments(intersection), atol=2e-13
                )
                self.assertTrue(
                    all(intersection.covers(Point(point)) for point in xy[at])
                )

    def test_subpixel_strip_does_not_disappear_or_become_opaque(self):
        polygon = np.array([[0, 0.125], [4, 0.125], [4, 0.12500001], [0, 0.12500001]])
        xy, ids, weight, _ = pixel_quadrature(polygon, [[0, 0], [4, 1]])
        self.assertEqual(len(np.unique(ids)), 4)
        np.testing.assert_allclose(np.bincount(ids, weights=weight), 1e-8, rtol=1e-8)
        self.assertTrue(np.all(xy[:, 1] > 0.125))
        self.assertTrue(np.all(xy[:, 1] < 0.12500001))

    def test_translation_empty_and_invalid(self):
        polygon = np.array([[0.1, 0.2], [0.9, 0.4], [0.3, 0.95]])
        a = pixel_quadrature(polygon, [[0, 0], [1, 1]])
        b = pixel_quadrature(polygon + 1000, [[1000, 1000], [1001, 1001]])
        np.testing.assert_allclose(a[0], b[0] - 1000, atol=2e-13)
        np.testing.assert_allclose(a[2], b[2], atol=2e-13)
        for points in [np.empty((0, 2)), [[0, 0], [1, 1]], polygon + 10]:
            xy, ids, weights, _ = pixel_quadrature(points, [[0, 0], [1, 1]])
            self.assertEqual((len(xy), len(ids), len(weights)), (0, 0, 0))
        with self.assertRaises(ValueError):
            pixel_quadrature([[0, np.nan]], [[0, 0], [1, 1]])
        with self.assertRaises(ValueError):
            pixel_quadrature(polygon, [[0.5, 0], [1, 1]])


if __name__ == "__main__":
    unittest.main()
