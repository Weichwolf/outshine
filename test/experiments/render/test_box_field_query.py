"""Ragged compact hierarchies, periodic gaps, exact boundaries and triangle controls."""

import unittest

import numpy as np
from box_field_query import BoxField
from triangle_image_reference import box_grid_source, intersect_source, prepare_source


class BoxFieldTests(unittest.TestCase):
    def test_direct_starts_at_the_three_dimensional_field_entry(self):
        field = BoxField(np.full((1000, 1), 6.0), 1.0, 1.0)
        origins = np.array([[0.5, -0.25, 100.0]])
        directions = np.array([[0.0, 1.0, -0.1]])
        depth, normal, work = field.trace_direct(origins, directions)
        np.testing.assert_allclose(depth, [940.0])
        np.testing.assert_array_equal(normal, [[0, 0, 1]])
        self.assertLessEqual(work["interval_ray_tests"], 2)

    def check_methods(self, field, origins, directions):
        a = field.trace_direct(origins, directions)
        b = field.trace_hierarchy(origins, directions)
        np.testing.assert_array_equal(np.isfinite(a[0]), np.isfinite(b[0]))
        finite = np.isfinite(a[0])
        np.testing.assert_allclose(a[0][finite], b[0][finite], rtol=1e-10, atol=1e-8)
        np.testing.assert_allclose(a[1][finite], b[1][finite], rtol=0, atol=1e-10)
        return b

    def test_random_ragged_fields_match_independent_triangles(self):
        rng = np.random.default_rng(174)
        for shape in ((3, 15), (15, 3), (1, 200), (200, 1), (1, 1)):
            for fill in (0.25, 0.75, 1.0):
                heights = rng.choice([0.0, 6.0, 12.0, 48.0], size=shape).astype(
                    np.float32
                )
                heights[0, 0] = 12
                field = BoxField(heights, 3.0, fill)
                origin = np.broadcast_to([shape[1] * 1.5, -3.0, 80.0], (128, 3))
                target = rng.uniform(
                    [0, 0, 0], [shape[1] * 3, shape[0] * 3, 1], (128, 3)
                )
                direction = target - origin
                direction /= np.linalg.norm(direction, axis=1)[:, None]
                depth, normal, _ = self.check_methods(field, origin, direction)
                source = box_grid_source(heights, field.pitch, field.fill)
                prepared = prepare_source(source)
                expected, faces = intersect_source(
                    source, origin, direction, prepared=prepared
                )
                np.testing.assert_array_equal(np.isfinite(depth), np.isfinite(expected))
                good = np.isfinite(expected)
                np.testing.assert_allclose(
                    depth[good], expected[good], rtol=1e-10, atol=1e-8
                )
                normals = prepared["normal"][faces[good]]
                normals /= np.linalg.norm(normals, axis=1)[:, None]
                np.testing.assert_allclose(normal[good], normals, rtol=0, atol=1e-10)

    def test_dense_palette_does_not_turn_padding_into_buildings(self):
        heights = np.full((1, 200), 48.0, np.float32)
        field = BoxField(heights, 3.0, 0.75)
        origin = np.broadcast_to([300.0, -3.0, 80.0], (512, 3))
        target = np.random.default_rng(81).uniform([0, 0, 0], [600, 3, 1], (512, 3))
        direction = target - origin
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        origin = np.concatenate((origin, [[3.0, -1.0, 4.0]]))
        direction = np.concatenate((direction, [[0.1, 1.0, 0.0]]))
        a = field.trace_direct(origin, direction)[0]
        b = field.trace_hierarchy(origin, direction)[0]
        source = box_grid_source(heights, field.pitch, field.fill)
        expected, _ = intersect_source(
            source, origin, direction, prepared=prepare_source(source)
        )
        np.testing.assert_array_equal(np.isfinite(a), np.isfinite(expected))
        np.testing.assert_array_equal(np.isfinite(b), np.isfinite(expected))

    def test_vertical_horizontal_and_inside_exit_boundaries(self):
        field = BoxField([[6, 0], [0, 12]], 2.0, 0.75)
        origins = [[1, 1, 10], [1, -2, 2], [-2, 1, 2], [1, 1, 2], [4, 4, 10]]
        directions = [[0, 0, -1], [0, 1, 0], [1, 0, 0], [0, 0, 1], [0, 0, -1]]
        depth, _, _ = self.check_methods(
            field, np.array(origins, float), np.array(directions, float)
        )
        np.testing.assert_allclose(depth, [4, 2.25, 2.25, 4, np.inf])

    def test_periodic_street_corridor_stays_empty(self):
        field = BoxField(np.full((127, 33), 48.0), 2.0, 0.75)
        origin = np.array([[16, -2, 8], [16, -2, 8]], float)
        direction = np.array([[0, 1, 0], [1e-5, 1, 0]], float)
        depth, _, _ = self.check_methods(field, origin, direction)
        self.assertTrue(np.isinf(depth).all())

    def test_packet_partition_preserves_hits_and_bounds_scratch(self):
        field = BoxField([[6, 0, 12], [48, 24, 0]], 4.0, 0.75)
        origin = np.broadcast_to([6.0, -3.0, 20.0], (31, 3))
        target = np.random.default_rng(42).uniform([0, 0, 0], [12, 8, 1], (31, 3))
        direction = target - origin
        a = field.trace_hierarchy(origin, direction)
        b = field.trace_hierarchy(origin, direction, packet_rays=7)
        np.testing.assert_array_equal(a[0], b[0])
        np.testing.assert_array_equal(a[1], b[1])
        self.assertLessEqual(b[2]["stack_bytes"], 7 * ((3 * field.top + 1) * 4 + 4))
        self.assertEqual(a[2]["node_ray_visits"], b[2]["node_ray_visits"])

    def test_ragged_palette_and_full_precision_storage(self):
        small = BoxField(np.ones((1, 200)), 1.0, 1.0)
        self.assertEqual(small.mips[0].dtype, np.uint8)
        np.testing.assert_array_equal(small.palette, [0.0, 1.0])
        widths = (200, 100, 50, 25, 13, 7, 4, 2, 1)
        self.assertEqual(small.retained_bytes, 2 * sum(w + 1 for w in widths) + 8)
        self.assertLess(small.retained_bytes, 8 * 200)
        heights = np.arange(289, dtype=np.float32).reshape(17, 17)
        field = BoxField(heights, 1.0, 0.75)
        self.assertIsNone(field.palette)
        self.assertEqual(field.mips[0].dtype, np.float32)
        np.testing.assert_array_equal(field.mips[0][:-1, :-1], heights)
        depth, _, _ = self.check_methods(
            field, np.array([[16.5, 16.5, 400.0]]), np.array([[0.0, 0.0, -1.0]])
        )
        self.assertEqual(depth[0], 112)

    def test_empty_field_and_empty_packet(self):
        field = BoxField(np.zeros((3, 5)), 1.0, 0.75)
        depth, _, work = field.trace_hierarchy([[1, -1, 10]], [[0, 1, -1]])
        self.assertTrue(np.isinf(depth).all())
        self.assertEqual(work["node_ray_visits"], 0)
        self.assertEqual(
            field.trace_hierarchy(np.empty((0, 3)), np.empty((0, 3)))[0].size, 0
        )

    def test_invalid_inputs_are_not_empty_geometry(self):
        for heights in ([], [[np.nan]], [[-1]], [[np.inf]]):
            with self.assertRaises(ValueError):
                BoxField(heights, 1.0, 0.75)
        field = BoxField([[6]], 1.0, 0.75)
        for method in (field.trace_direct, field.trace_hierarchy):
            with self.assertRaises(ValueError):
                method([[1, 1, 1]], [[0, 0, 0]])
            with self.assertRaises(ValueError):
                method([[np.nan, 1, 1]], [[0, 1, 0]])
        with self.assertRaises(ValueError):
            field.trace_hierarchy([[1, 1, 1]], [[0, 1, 0]], packet_rays=0)


if __name__ == "__main__":
    unittest.main()
