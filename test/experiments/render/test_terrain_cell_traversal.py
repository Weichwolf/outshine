"""Analytic first hits, source-preserving bounds, actual native inputs and independent raster."""

import unittest

import numpy as np
from current_visibility import project, raster
from terrain_cell_traversal import node_bounds, prepare, sample
from terrain_cell_traversal_experiment import camera, source_page
from terrain_grid_lod import triangles
from terrain_occluders import skirts

METHODS = ("scan", "cells", "tree")


def grid(height):
    y, x = np.indices(height.shape, dtype=float)
    return np.stack((x, -y, height), axis=-1).reshape(-1, 3)


class TerrainCellTraversal(unittest.TestCase):
    def compare(self, vertices, faces, side, origins, directions, **kwargs):
        outputs = []
        for method in METHODS:
            result = sample(
                prepare(vertices, faces, side, strategy=method),
                origins,
                directions,
                **kwargs,
            )
            if outputs:
                for key in ("ray_t", "face_ids", "barycentric", "valid"):
                    np.testing.assert_array_equal(result[key], outputs[0][key])
            outputs.append(result)
        return outputs

    def test_dyadic_plane_has_analytic_height_and_exact_source_witness(self):
        y, x = np.indices((9, 9), dtype=float)
        v = grid(2 + 0.25 * x - 0.125 * y)
        origins = np.array([[1.25, -2.25, 5.0], [4.5, -0.5, 5.0], [25.0, -2.0, 5.0]])
        result = self.compare(
            v, triangles(9, 1), 9, origins, np.tile([0.0, 0.0, -1.0], (3, 1))
        )[0]
        np.testing.assert_array_equal(result["ray_t"], [2.96875, 1.9375, np.inf])
        np.testing.assert_array_equal(result["face_ids"], [34, 8, -1])

    def test_diagonal_edges_vertices_and_signed_zero_do_not_fall_through(self):
        v = grid(np.zeros((5, 5)))
        o = np.array(
            [[0.0, 0.0, 2.0], [4.0, -4.0, 2.0], [2.0, -2.0, 2.0], [4.0, -1.5, 2.0]]
        )
        result = self.compare(
            v, triangles(5, 1), 5, o, np.tile([-0.0, 0.0, -1.0], (4, 1))
        )[0]
        np.testing.assert_array_equal(result["ray_t"], 2)
        self.assertTrue(result["valid"].all())

    def test_grazing_ray_keeps_first_crossing_and_interval_can_select_later_one(self):
        v = grid(np.tile([0.0, 2.0, 0.0, 2.0, 0.0], (5, 1)))
        o, d = np.array([[-1.0, -0.25, 1.0]]), np.array([[1.0, 0.0, 0.0]])
        self.assertEqual(
            self.compare(v, triangles(5, 1), 5, o, d)[0]["ray_t"].item(), 1.5
        )
        self.assertEqual(
            self.compare(v, triangles(5, 1), 5, o, d, t_min=1.6)[0]["ray_t"].item(), 3.5
        )
        self.assertFalse(
            self.compare(v, triangles(5, 1), 5, o, d, t_max=1.4)[0]["valid"].item()
        )

    def test_bilinear_substitution_would_change_the_actual_triangle_surface(self):
        v = grid(np.array([[0.0, 0.0], [0.0, 1.0]]))
        result = self.compare(
            v, triangles(2, 1), 2, [[0.25, -0.75, 2.0]], [[0.0, 0.0, -1.0]]
        )[0]
        self.assertEqual(result["ray_t"].item(), 1.75)
        self.assertNotEqual(result["ray_t"].item(), 2 - 0.25 * 0.75)

    def test_one_sided_surface_parallel_below_and_zero_parameter_are_explicit(self):
        v = grid(np.zeros((3, 3)))
        o = [
            [0.25, -0.25, -1.0],
            [0.25, -0.25, -1.0],
            [0.25, -0.25, 0.0],
            [0.25, -0.25, 1.0],
        ]
        d = [[0.0, 0.0, 1.0], [0.0, 0.0, -1.0], [0.0, 0.0, -1.0], [1.0, 0.0, 0.0]]
        result = self.compare(v, triangles(3, 1), 3, o, d)[0]
        self.assertFalse(result["valid"].any())
        self.assertTrue(np.isinf(result["ray_t"]).all())
        self.assertTrue(np.isnan(result["barycentric"]).all())

    def test_side_entry_still_hits_the_actual_outer_skirt_before_any_top(self):
        v, extra, _ = skirts(grid(np.zeros((9, 9))), 16, side=9)
        faces = np.vstack((triangles(9, 1), extra))
        result = self.compare(v, faces, 9, [[-2.0, -4.25, -3.0]], [[1.0, 0.0, 0.0]])[0]
        self.assertEqual(result["ray_t"].item(), 2)
        self.assertGreaterEqual(result["face_ids"].item(), 128)

    def test_compact_bounds_enclose_all_actual_nonaffine_sagged_source_vertices(self):
        y, x = np.indices((9, 9), dtype=float)
        v = np.stack(
            (
                x + 0.02 * x * y,
                -y + 0.01 * x * y,
                np.sin(x) + 0.125 * y - 0.01 * (x * x + y * y),
            ),
            axis=-1,
        ).reshape(-1, 3)
        model = prepare(v, triangles(9, 1), 9)
        for level in range(4):
            span = 1 << level
            n = 8 // span
            rows, columns = np.indices((n, n))
            low, high = node_bounds(model, level, rows.ravel(), columns.ravel())
            for i, (row, col) in enumerate(zip(rows.ravel(), columns.ravel())):
                source = v.reshape(9, 9, 3)[
                    row * span : (row + 1) * span + 1, col * span : (col + 1) * span + 1
                ]
                self.assertTrue(np.all(source >= low[i]))
                self.assertTrue(np.all(source <= high[i]))

    def test_actual_native_page_matches_independent_perspective_raster_including_skirts(
        self,
    ):
        v, faces, _ = source_page()
        view, origins, directions = camera(v, 32, 18)
        projected, z = project(v, view)
        self.assertTrue((z > 0.1).all())
        depth = raster(projected, z, faces, 32, 18)
        result = self.compare(v, faces, 33, origins, directions)[0]
        np.testing.assert_array_equal(result["valid"].reshape(depth.shape), depth > 0)
        forward_z = result["ray_t"] * (directions @ view[1][2])
        inverse = np.divide(
            0.1, forward_z, out=np.zeros(len(origins)), where=result["valid"]
        )
        np.testing.assert_allclose(
            inverse.reshape(depth.shape), depth, atol=1e-12, rtol=0
        )

    def test_all_compact_node_bounds_include_the_actual_deep_outer_faces(self):
        v, extra, bottom = skirts(grid(np.zeros((9, 9))), 16, side=9)
        model = prepare(v, np.vstack((triangles(9, 1), extra)), 9)
        source = v[:81].reshape(9, 9, 3)
        for level in range(4):
            span = 1 << level
            for row in range(8 // span):
                for col in range(8 // span):
                    lo, hi = node_bounds(model, level, np.array([row]), np.array([col]))
                    r, c = row * span, col * span
                    points = [source[r : r + span + 1, c : c + span + 1].reshape(-1, 3)]
                    if r == 0:
                        points.append(bottom[0, c : c + span + 1])
                    if r + span == 8:
                        points.append(bottom[1, c : c + span + 1])
                    if c == 0:
                        points.append(bottom[2, r : r + span + 1])
                    if c + span == 8:
                        points.append(bottom[3, r : r + span + 1])
                    points = np.concatenate(points)
                    self.assertTrue(np.all(points >= lo))
                    self.assertTrue(np.all(points <= hi))
        v[-1, 0] += 0.125
        with self.assertRaisesRegex(ValueError, "outer-skirt layout"):
            prepare(v, np.vstack((triangles(9, 1), extra)), 9)

    def test_ray_parameter_scales_with_direction_and_counters_are_serializable(self):
        v, extra, _ = skirts(grid(np.zeros((9, 9))), 16, side=9)
        faces = np.vstack((triangles(9, 1), extra))
        for method in METHODS:
            model = prepare(v, faces, 9, strategy=method)
            a = sample(model, [[0.25, -0.25, 2.0]], [[0.0, 0.0, -1.0]])
            b = sample(model, [[0.25, -0.25, 2.0]], [[0.0, 0.0, -2.0]])
            np.testing.assert_array_equal(a["ray_t"], 2 * b["ray_t"])
            np.testing.assert_array_equal(a["face_ids"], b["face_ids"])
            self.assertTrue(all(type(value) is int for value in a["work"].values()))
            empty = sample(model, np.empty((0, 3)), np.empty((0, 3)))
            self.assertEqual(empty["valid"].size, 0)
            with self.assertRaisesRegex(ValueError, "cancelled"):
                sample(
                    model, np.empty((0, 3)), np.empty((0, 3)), cancelled=lambda: True
                )

    def test_model_owns_immutable_source_and_query_partition_keeps_results(self):
        v, f = grid(np.zeros((9, 9))), triangles(9, 1)
        model = prepare(v, f, 9)
        o = np.array([[1.25, -2.25, 2.0]] * 70)
        d = np.tile([0.0, 0.0, -1.0], (70, 1))
        before = sample(model, o, d)
        v[:] = 0
        f[:] = 0
        after = sample(model, o, d, batch_size=7)
        for key in ("ray_t", "face_ids", "barycentric", "valid"):
            np.testing.assert_array_equal(before[key], after[key])
        self.assertFalse(model.mesh.vertices.flags.writeable)
        self.assertTrue(all(not b.flags.writeable for b in model.height_bounds[1:]))

    def test_budgets_cancellation_and_bad_inputs_publish_no_false_miss(self):
        v, f = grid(np.zeros((3, 3))), triangles(3, 1)
        o, d = np.array([[0.25, -0.25, 2.0]]), np.array([[0.0, 0.0, -1.0]])
        for method in METHODS:
            model = prepare(v, f, 3, strategy=method)
            for budget in (
                {"max_rays": 0},
                {"max_pairs": 0},
                {"max_work": 0},
                {"cancelled": lambda: True},
            ):
                with self.assertRaises(ValueError):
                    sample(model, o, d, **budget)
            for bad in ([[0.0, 0.0, 0.0]], [[0.0, np.nan, -1.0]]):
                with self.assertRaises(ValueError):
                    sample(model, o, bad)
            self.assertTrue(sample(model, o, d)["valid"].item())
        with self.assertRaises(ValueError):
            prepare(v, f[::-1], 3)


if __name__ == "__main__":
    unittest.main()
