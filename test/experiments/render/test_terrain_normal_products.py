"""Float32 word identity, source/metric binding and unchanged fine interpolation."""

import hashlib
import json
import unittest
from unittest.mock import patch

import numpy as np
from terrain_cell_traversal_experiment import FIXTURE
from terrain_grid_lod import page_frame, page_steps, read_page
from terrain_normal_products import bind, build, sample, sample_fine, source
from terrain_normal_products_experiment import unique_words


def real_source():
    record = json.loads((FIXTURE / "input.json").read_text())
    row = record["row"]
    heights, _ = read_page(row, FIXTURE)
    if (
        hashlib.sha256(heights.astype("<f4").tobytes()).hexdigest()
        != record["height_sha256"]
    ):
        raise ValueError("changed pinned source height words")
    binding = (record["asset_key"], int(row["page"]), row["zoom"], row["x"], row["y"])
    return source(heights, page_steps(page_frame(row)[2], 33), binding=binding), record


class TerrainNormalProducts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.record = real_source()
        y, x = np.indices((33, 33))
        cls.nodes = np.column_stack((y.ravel(), x.ravel()))
        cls.uv = np.random.default_rng(1241).random((4096, 2)).astype(np.float32)

    def compare(self, src, *, basis=None):
        result = sample(bind(src, basis=basis), self.nodes)
        for kind in ("differences", "rgba_normals"):
            cached = sample(bind(src, build(src, kind), basis=basis), self.nodes)
            np.testing.assert_array_equal(
                cached["normals"].view(np.uint32), result["normals"].view(np.uint32)
            )
            self.assertEqual(cached["binding"], src.binding)
        return result["normals"]

    def test_every_actual_node_keeps_the_existing_source_normal_words(self):
        self.compare(self.source)
        self.assertEqual(self.source.heights.nbytes, 4900)
        self.assertEqual(
            sum(f.nbytes for f in build(self.source, "differences").fields), 8712
        )
        self.assertEqual(
            sum(f.nbytes for f in build(self.source, "rgba_normals").fields), 17424
        )

    def test_source_north_sign_and_metric_clamp_have_independent_dyadic_control(self):
        row, col = np.indices((35, 35), dtype=np.float32)
        src = source(
            np.float32(100) + col * np.float32(0.25) + row * np.float32(0.125), [2, 2]
        )
        normal = self.compare(src)
        np.testing.assert_allclose(
            normal[:, 0] / normal[:, 2], -0.125, atol=1e-7, rtol=0
        )
        np.testing.assert_allclose(
            normal[:, 1] / normal[:, 2], 0.0625, atol=1e-7, rtol=0
        )
        narrow = source(src.heights, [1e-7, 1e-6])
        np.testing.assert_array_equal(narrow.step_m, np.float32(0.001))
        self.compare(narrow)

    def test_flat_source_and_halo_extrema_do_not_wrap_to_opposite_edges(self):
        flat = np.full((35, 35), 128, dtype=np.float32)
        normal = self.compare(source(flat, [1, 1]))
        np.testing.assert_array_equal(normal, np.tile([0, 0, 1], (1089, 1)))
        flat[0, 1:-1], flat[-1, 1:-1] = 129, 126
        flat[1:-1, 0], flat[1:-1, -1] = 132, 127
        normal = self.compare(source(flat, [1, 1])).reshape(33, 33, 3)
        self.assertGreater(normal[0, 16, 1], -1)
        self.assertLess(normal[0, 16, 1], 0)
        self.assertLess(normal[16, 0, 0], 1)
        self.assertGreater(normal[16, 0, 0], 0)
        np.testing.assert_array_equal(
            normal[1:-1, 1:-1], np.broadcast_to([0, 0, 1], (31, 31, 3))
        )

    def test_rotated_and_nonorthonormal_source_bases_keep_all_words(self):
        for basis in (
            np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]], dtype=np.float32),
            np.array(
                [[1.5, 0.125, 0], [0, 0.75, 0.25], [0.25, 0, 2]], dtype=np.float32
            ),
        ):
            self.compare(self.source, basis=basis)

    def test_fine_triangles_and_clamped_uv_keep_both_diagonals_and_endpoints(self):
        uv = np.vstack((self.uv, [[0, 0], [1, 1], [0.5, 0.5], [-0.25, 1.25]])).astype(
            np.float32
        )
        basis = np.array(
            [[1.5, 0.125, 0], [0, 0.75, 0.25], [0.25, 0, 2]], dtype=np.float32
        )
        reference = sample_fine(bind(self.source, basis=basis), uv)["normals"]
        for kind in ("differences", "rgba_normals"):
            result = sample_fine(
                bind(self.source, build(self.source, kind), basis=basis), uv
            )
            np.testing.assert_array_equal(
                result["normals"].view(np.uint32), reference.view(np.uint32)
            )
        edge = sample_fine(
            bind(self.source, basis=basis), np.array([[0, 1]], dtype=np.float32)
        )["normals"]
        np.testing.assert_array_equal(reference[-1], edge[0])

    def test_metric_changes_reuse_differences_but_refuse_ready_local_normals(self):
        changed = source(
            self.source.heights,
            self.source.step_m * np.float32(2),
            binding=self.source.binding,
        )
        delta = build(self.source, "differences")
        np.testing.assert_array_equal(
            sample(bind(changed, delta), self.nodes)["normals"],
            sample(bind(changed), self.nodes)["normals"],
        )
        with self.assertRaisesRegex(ValueError, "stale"):
            bind(changed, build(self.source, "rgba_normals"))

    def test_halo_change_invalidates_both_prepared_products(self):
        altered = self.source.heights.copy()
        altered[0, 16] += np.float32(1)
        changed = source(altered, self.source.step_m, binding=self.source.binding)
        self.assertNotEqual(changed.digest, self.source.digest)
        before = sample(bind(self.source), self.nodes)["normals"]
        after = sample(bind(changed), self.nodes)["normals"]
        self.assertEqual(np.count_nonzero(np.any(before != after, axis=1)), 1)
        for kind in ("differences", "rgba_normals"):
            with self.assertRaisesRegex(ValueError, "stale"):
                bind(changed, build(self.source, kind))

    def test_snapshot_and_prepared_fields_own_their_immutable_words(self):
        heights = self.source.heights.copy()
        src = source(heights, self.source.step_m, binding=(2**64 - 1, "actual-page"))
        product = build(src, "rgba_normals")
        lookup = bind(src, product)
        before = sample(lookup, self.nodes)["normals"]
        heights[:] = 0
        np.testing.assert_array_equal(sample(lookup, self.nodes)["normals"], before)
        self.assertFalse(src.heights.flags.writeable)
        self.assertFalse(src.step_m.flags.writeable)
        self.assertTrue(all(not f.flags.writeable for f in product.fields))
        self.assertEqual(sample(lookup, self.nodes)["binding"][0], 2**64 - 1)

    def test_sampling_does_not_hash_source_again(self):
        lookups = [bind(self.source)] + [
            bind(self.source, build(self.source, kind))
            for kind in ("differences", "rgba_normals")
        ]
        with patch(
            "terrain_normal_products.hashlib.sha256",
            side_effect=AssertionError("query must not hash source"),
        ):
            for lookup in lookups:
                self.assertEqual(sample(lookup, self.nodes)["normals"].shape, (1089, 3))

    def test_distinct_height_neighbours_reuse_words_needed_by_positions(self):
        height_addresses = set()
        for row, col in self.nodes:
            height_addresses.update(
                (
                    (row + 1, col + 2),
                    (row + 1, col),
                    (row, col + 1),
                    (row + 2, col + 1),
                    (row + 1, col + 1),
                )
            )
        counts = unique_words(self.nodes)["combined_normal_and_position_or_receiver"]
        self.assertEqual(len(height_addresses), 1221)
        self.assertEqual(
            counts["height_stencil"]["different_texture_bytes"],
            4 * len(height_addresses),
        )
        self.assertEqual(counts["differences"]["different_texture_bytes"], 13068)
        self.assertEqual(counts["rgba_normals"]["different_texture_bytes"], 21780)

    def test_budgets_cancellation_and_invalid_queries_publish_no_normal(self):
        for product in (
            None,
            build(self.source, "differences"),
            build(self.source, "rgba_normals"),
        ):
            lookup = bind(self.source, product)
            for args in (
                {"max_queries": 0},
                {"max_words": 0},
                {"cancelled": lambda: True},
            ):
                with self.assertRaises(ValueError):
                    sample(lookup, [[0, 0]], **args)
            for bad in ([[33, 0]], [[-1, 0]], [[0.0, 0.5]]):
                with self.assertRaises(ValueError):
                    sample(lookup, bad)
            empty = sample(lookup, np.empty((0, 2), dtype=np.int32))["normals"]
            self.assertEqual(empty.shape, (0, 3))
        with self.assertRaises(ValueError):
            source(self.source.heights.astype(np.float64), self.source.step_m)
        with self.assertRaises(ValueError):
            bind(self.source, basis=np.zeros((3, 3), dtype=np.float32))


if __name__ == "__main__":
    unittest.main()
