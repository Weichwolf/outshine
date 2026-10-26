"""Signed-composition and projected-support controls, separate from image fit."""

import unittest

import numpy as np
from compare_urban_parents import camera
from shapely.geometry import Point, Polygon
from urban_parent_reference import support_points
from urban_pixel_filter import (
    filter_correction,
    screen_rays,
    support_polygon,
)


class UrbanPixelTest(unittest.TestCase):
    def test_full_pixels_retain_original_nodes_and_partial_pixels_keep_area(self):
        polygon = np.array([[0, 0], [1.5, 0], [1.5, 1], [0, 1]])
        bounds = np.array([[0, 0], [2, 1]])
        view = (np.array([0, 0, 2]), np.eye(3), 2.0, 2, 1)
        xy = np.array([[0.2, 0.3], [0.8, 0.9], [1.1, 0.2], [1.6, 0.8]])
        o, d = screen_rays(view, xy)
        visited = []

        def scene(origins, directions):
            visited.extend(directions.copy())
            return directions**2, np.ones(len(origins)), {}

        def background(origins, directions):
            return np.zeros_like(origins)

        delta, coverage, work = filter_correction(
            polygon,
            bounds,
            view,
            scene,
            background,
            order=2,
            interior=(o, d, np.array([0, 0, 1, 1])),
        )
        np.testing.assert_allclose(delta[0, 0], (d[:2] ** 2).mean(axis=0), atol=1e-16)
        np.testing.assert_array_equal(np.asarray(visited)[-2:], d[:2])
        np.testing.assert_allclose(coverage, [[1, 0.5]], atol=1e-16)
        self.assertEqual(work["retained_full_pixels"], 1)
        self.assertEqual(work["retained_uniform_rays"], 2)

    def test_signed_difference_keeps_area_and_background_separate(self):
        polygon = np.array([[0, 0.1], [2, 0.1], [2, 0.2], [0, 0.2]])
        view = (np.array([0, 0, 2]), np.eye(3), 2.0, 2, 1)
        difference = np.array([-0.25, 0.5, 0.1])

        def background(o, d):
            return np.full_like(o, 2.0) + d * 0.01

        def scene(o, d):
            return (
                background(o, d) + difference,
                np.full(len(o), 0.6),
                {"queries": len(o)},
            )

        delta, coverage, work = filter_correction(
            polygon, np.array([[0, 0], [2, 1]]), view, scene, background, order=2
        )
        np.testing.assert_allclose(
            delta, np.broadcast_to(0.1 * difference, (1, 2, 3)), atol=1e-16
        )
        np.testing.assert_allclose(coverage, 0.06, atol=1e-16)
        self.assertEqual(work["transport"]["queries"], 8)
        self.assertEqual(work["clipped_pixels"], 2)

    def test_support_contains_box_and_projected_cast_shadows(self):
        product = {
            "reference_origin_m": np.zeros(3),
            "shape": np.array([8, 7]),
            "header": {"pitch_m": 10.0},
        }
        height = 25.0
        view = camera(80, 1000, 0)
        rng = np.random.default_rng(1113)
        points = rng.uniform([0, 0, 0], [80, 70, height], (300, 3))
        for profile in [
            {"kind": "sun", "direction": [0.5, -0.2, 0.1]},
            {"kind": "point", "position": [30, 20, 100]},
        ]:
            hull = Polygon(
                support_polygon(support_points(product, height, profile), view)
            )
            if profile["kind"] == "sun":
                direction = np.broadcast_to(profile["direction"], points.shape)
            else:
                direction = np.asarray(profile["position"]) - points
            ground = points - points[:, 2, None] * direction / direction[:, 2, None]
            p = (np.vstack((points, ground)) - view[0]) @ view[1].T
            screen = [640, 360] + view[2] * p[:, :2] / p[:, 2, None] * [1, -1]
            self.assertTrue(all(hull.covers(Point(x)) for x in screen))

    def test_empty_support_skips_queries_and_invalid_light_is_explicit(self):
        def forbidden(o, d):
            self.fail("outside-support callbacks must not run")

        view = (np.zeros(3), np.eye(3), 1.0, 1, 1)
        delta, coverage, work = filter_correction(
            np.array([[3, 3], [4, 3], [4, 4]]),
            np.array([[0, 0], [1, 1]]),
            view,
            forbidden,
            forbidden,
            order=2,
        )
        self.assertEqual(np.count_nonzero(delta) + np.count_nonzero(coverage), 0)
        self.assertEqual(work["quadrature_samples"], 0)
        product = {
            "reference_origin_m": np.zeros(3),
            "shape": np.array([1, 1]),
            "header": {"pitch_m": 1.0},
        }
        with self.assertRaises(ValueError):
            support_points(product, 10, {"kind": "point", "position": [0, 0, 5]})


if __name__ == "__main__":
    unittest.main()
