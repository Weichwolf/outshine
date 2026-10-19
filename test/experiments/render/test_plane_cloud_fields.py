"""Independent analytic ray, hidden-side and original-fallback controls."""

import unittest

import numpy as np
from object_plane_cloud import partition
from plane_cloud_fields import bake, query
from test_object_plane_cloud import combine, squares
from triangle_image_reference import bilinear, prepare_source


def source(parts):
    p, t = combine(parts)
    normals = np.repeat([[0.0, 0.0, 1.0]], len(p), axis=0)
    return {
        "positions": p,
        "indices": t,
        "vertex_normal": normals,
        "colour": np.ones((len(p), 4)),
        "uv": np.zeros((len(p), 2)),
        "metres": np.zeros((len(p), 2)),
    }


class PlaneFields(unittest.TestCase):
    def test_filter_centres_boundary_and_zero_exterior(self):
        values = np.array([[[1.0], [2.0]], [[3.0], [4.0]]])
        uv = np.array([[0.25, 0.25], [0.5, 0.5], [0.0, 0.0], [-1.0, 0.0]])
        np.testing.assert_array_equal(bilinear(values, uv)[:, 0], [1.0, 2.5, 0.25, 0.0])

    def test_exact_coplanar_interior_and_guard_empty_space(self):
        data = source([squares(0), squares(0)])
        charts, fallback, _, _ = partition(
            data["positions"], data["indices"], epsilon=0.001, pitch=0.125
        )
        self.assertEqual(len(fallback), 0)
        baked = [bake(data, chart) for chart in charts]
        origins = np.array([[0.5, 0.5, 1.0], [2.0, 2.0, 1.0]])
        directions = np.array([[0.0, 0.0, -1.0]] * 2)
        depth, alpha, rgb, _ = query(
            baked, None, None, origins, directions, [[0, 0, 1], [0, 0, -1]]
        )
        np.testing.assert_array_equal(alpha, [1, 0])
        np.testing.assert_array_equal(depth, [1, np.inf])
        np.testing.assert_allclose(rgb[:, 0], [[1, 1, 1], [0.2, 0.2, 0.2]])
        np.testing.assert_array_equal(rgb[:, 1], 0)

    def test_backside_is_represented_independently_of_front_view(self):
        data = source(
            [squares(0), squares(0), squares(-0.1, True), squares(-0.1, True)]
        )
        data["vertex_normal"][8:] = [0, 0, -1]
        data["colour"][8:, :3] = [0, 0, 1]
        charts, _, _, _ = partition(
            data["positions"], data["indices"], epsilon=0.001, pitch=0.125
        )
        baked = [bake(data, chart) for chart in charts]
        depth, alpha, rgb, _ = query(
            baked,
            None,
            None,
            np.array([[0.5, 0.5, -1.0]]),
            np.array([[0.0, 0.0, 1.0]]),
            [[0, 0, -1]],
        )
        np.testing.assert_allclose(depth, [0.9])
        np.testing.assert_array_equal(alpha, [1])
        np.testing.assert_allclose(rgb, [[[0, 0, 1]]])

    def test_budget_fallback_uses_original_geometry_and_fields(self):
        data = source([squares(0)])
        charts, fallback, _, _ = partition(
            data["positions"], data["indices"], epsilon=0.1, pitch=0.1, plane_limit=0
        )
        self.assertEqual(charts, ())
        self.assertEqual(fallback.tolist(), [0, 1])
        depth, alpha, rgb, _ = query(
            [],
            data,
            prepare_source(data),
            np.array([[0.5, 0.5, 1.0]]),
            np.array([[0.0, 0.0, -1.0]]),
            [[0, 0, 1]],
        )
        np.testing.assert_array_equal(depth, [1])
        np.testing.assert_array_equal(alpha, [1])
        np.testing.assert_array_equal(rgb, [[[1, 1, 1]]])

    def test_stored_geometry_is_not_a_camera_facing_plane(self):
        data = source([squares(0), squares(0)])
        charts, _, _, _ = partition(
            data["positions"], data["indices"], epsilon=0.001, pitch=0.125
        )
        baked = [bake(data, chart) for chart in charts]
        o = np.array([[0.2, 0.5, 1.0], [0.3, 0.5, 1.0]])
        d = np.array([[0.2, 0.0, -1.0], [0.1, 0.0, -1.0]])
        depth, alpha, _, _ = query(baked, None, None, o, d, [[0, 0, 1]])
        np.testing.assert_array_equal(depth, [1, 1])
        np.testing.assert_array_equal(alpha, [1, 1])


if __name__ == "__main__":
    unittest.main()
