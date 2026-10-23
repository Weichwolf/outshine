"""Physical flat-model shadow bounds and empty-field diagnostic projection."""

import unittest

import numpy as np
from test_urban_parent_stats import example
from urban_parent_reference import projected_region, support_points


def product():
    return {
        "reference_origin_m": np.array([-16.0, 0, 0]),
        "shape": np.array([40, 32]),
        "header": {"pitch_m": 16.0},
    }


class ParentProjectionTest(unittest.TestCase):
    def test_shadow_corners_are_on_the_ground_and_collinear_with_the_light(self):
        for profile in [
            {"kind": "sun", "direction": [1.0, -2, 3]},
            {"kind": "point", "position": [200.0, -300, 1000]},
        ]:
            p = support_points(product(), 48, profile)
            np.testing.assert_array_equal(p[8:, 2], 0)
            light = (
                np.array(profile["direction"])
                if profile["kind"] == "sun"
                else np.array(profile["position"]) - p[4:8]
            )
            delta = p[8:] - p[4:8]
            np.testing.assert_allclose(np.cross(delta, light), 0, atol=1e-8, rtol=0)
            self.assertTrue(np.all(np.sum(delta * light, axis=1) < 0))

    def test_projected_support_fits_the_view_and_ignores_empty_field_bounds(self):
        parent = example()
        view = (np.array([256.0, 256, 2400]), np.diag([1.0, -1, -1]), 800, 1280, 720)
        profile = {"kind": "sun", "direction": [1, -2, 3]}
        box = projected_region(parent, product(), view, profile)
        points = support_points(product(), parent.heights[-1], profile)
        p = (points - view[0]) @ view[1].T
        xy = [640, 360] + view[2] * p[:, :2] / p[:, 2, None] * [1, -1]
        self.assertTrue(np.all(xy >= box[0]) and np.all(xy <= box[1]))
        empty = product() | {
            "shape": np.zeros(2, int),
            "reference_origin_m": np.array([1e12, 1e12, -1e12]),
        }
        first = projected_region(parent, empty, view, profile)
        second = projected_region(
            parent, empty | {"reference_origin_m": np.zeros(3)}, view, profile
        )
        np.testing.assert_array_equal(first, second)
        self.assertTrue(np.all(first >= 0))
        self.assertTrue(np.all(first[1] <= [1280, 720]))

    def test_unbounded_flat_shadow_inputs_are_rejected(self):
        for profile in [
            {"kind": "sun", "direction": [1, 1, 0]},
            {"kind": "point", "position": [0, 0, 48]},
        ]:
            with self.assertRaises(ValueError):
                support_points(product(), 48, profile)


if __name__ == "__main__":
    unittest.main()
