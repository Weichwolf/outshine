"""Exact original words, adaptive choices, selected reads and bounded failures."""

import gc
import unittest
import weakref
from unittest.mock import patch

import attribute_words as rows
import compact_fragment_inputs as compact
import numpy as np
import source_fragment_inputs as fragments
from test_source_fragment_inputs import CAPTURE, inputs, source


class AttributeWordsTests(unittest.TestCase):
    def test_compact_product_does_not_retain_plain_attribute_snapshots(self):
        material, _ = inputs(source(), np.array([[0.0, 0.0]]))
        refs = [weakref.ref(material.source[key]) for key in compact.KEYS]
        candidate = compact.compact(material)
        del material
        gc.collect()
        self.assertTrue(all(ref() is None for ref in refs))
        self.assertEqual(candidate.source["colour"].shape[1], 4)

    def test_selected_corner_layout_preserves_consumer_reduction_order(self):
        rng = np.random.default_rng(47)
        weights = rng.random((512, 3))
        weights /= weights.sum(1, keepdims=True)
        selected = rng.integers(0, 701, (512, 3))
        a = np.tile(np.array([[1.0, -0.0, 2.0, 3.2]], np.float32), (701, 1))
        for width in (2, 3, 4):
            values = a[:, :width].copy()
            for mode in ("adaptive", "palette16", "palette32"):
                decoded = rows.prepare(values, mode)[selected]
                self.assertTrue(decoded.flags.c_contiguous)
                expected = np.einsum("ij,ijk->ik", weights, values[selected])
                actual = np.einsum("ij,ijk->ik", weights, decoded)
                np.testing.assert_array_equal(
                    actual.view(np.uint64), expected.view(np.uint64)
                )

    def test_signed_zero_hdr_alpha_and_nan_bits_survive_every_representation(self):
        words = np.array(
            [
                [0, 0x80000000, 0x3FC4EC4F, 0x7FC00001],
                [0x7FC00002, 0, 0xFF800000, 0x3F000000],
            ],
            np.uint32,
        )
        for width in (1, 2, 3, 4):
            a = words[[0, 1, 0, 1, 1, 0, 1], :width].copy().view(np.float32)
            for mode in ("direct", "palette16", "palette32", "adaptive"):
                encoded = rows.prepare(a, mode)
                np.testing.assert_array_equal(
                    encoded[np.arange(len(a))].view(np.uint32), a.view(np.uint32)
                )
                chosen = np.array([[6, 0], [1, 2]], np.uint32)
                np.testing.assert_array_equal(
                    encoded[chosen].view(np.uint32), a[chosen].view(np.uint32)
                )
                np.testing.assert_array_equal(
                    encoded[chosen, 0].view(np.uint32), a[chosen, 0].view(np.uint32)
                )

    def test_constant_has_no_index_array_and_direct_fallback_does_not_grow_data(self):
        a = np.tile(np.array([[1.0, -0.0, 2.0]], np.float32), (701, 1))
        value = rows.prepare(a)
        self.assertEqual(value.mode, "constant")
        self.assertIsNone(value.indices)
        self.assertEqual(value.nbytes, 12)
        np.testing.assert_array_equal(
            value[np.arange(len(a))].view(np.uint32), a.view(np.uint32)
        )
        a = np.arange(1000, dtype=np.float32)[:, None]
        value = rows.prepare(a)
        self.assertEqual(value.mode, "direct")
        self.assertEqual(value.nbytes, a.nbytes)
        empty = rows.prepare(np.empty((0, 2), np.float32))
        self.assertEqual(empty[np.array([], np.int64)].shape, (0, 2))

    def test_auto_index_width_does_not_truncate_65537_distinct_words(self):
        words = np.zeros((66000 * 3, 4), np.uint32)
        words[:, 0] = np.tile(np.arange(66000, dtype=np.uint32), 3)
        a = words.view(np.float32)
        with self.assertRaises(ValueError):
            rows.prepare(a, "palette16")
        value = rows.prepare(a)
        self.assertEqual(value.mode, "palette32")
        np.testing.assert_array_equal(
            value[np.array([0, 65536, 131999, 197999])].view(np.uint32),
            words[[0, 65536, 131999, 197999]],
        )

    def test_malformed_spans_indices_layout_and_budget_refuse_a_product(self):
        a = np.zeros((7, 3), np.float32)
        value = rows.prepare(a, "palette16")
        for payload in (value.payload[:-1], value.payload + bytes(16)):
            with self.assertRaises(ValueError):
                rows.restore(payload, 7, 3, "palette16")
        bad = np.frombuffer(value.payload, np.uint32).copy()
        bad[4] = 99
        with self.assertRaises(ValueError):
            rows.restore(bad.tobytes(), 7, 3, "palette16")
        for count, width, mode in (
            (8, 3, "palette16"),
            (7, 3, "palette32"),
            (7, 4, "constant"),
        ):
            with self.assertRaises(ValueError):
                rows.restore(value.payload, count, width, mode)
        with self.assertRaises(rows.AttributeLimit):
            rows.prepare(a, "direct", bytes_most=1)
        for bad in (
            np.array([-1]),
            np.array([7]),
            np.array([0.1]),
            np.array([2**64 - 1], np.uint64),
            np.zeros(rows.MAX_ROWS + 1, np.uint32),
        ):
            with self.assertRaises(ValueError):
                value[bad]

    def test_views_and_selected_gathers_do_not_depend_on_full_decode_or_mutable_input(
        self,
    ):
        a = np.tile(np.array([[1, 2, 3]], np.float32), (17, 1))
        with patch(
            "vertex_colour_palette.decode", side_effect=AssertionError("whole decode")
        ):
            value = rows.prepare(a, "palette16")
            payload = bytearray(value.payload)
            restored = rows.restore(payload, 17, 3, "palette16")
            payload[:] = bytes(len(payload))
            a[:] = 0
            np.testing.assert_array_equal(
                restored[np.array([16, 0])], [[1, 2, 3], [1, 2, 3]]
            )
        with self.assertRaises(ValueError):
            restored.table.setflags(write=True)

    def test_material_reader_outputs_and_shared_geometry_do_not_change(self):
        s = source(rear=True)
        points = np.array([[1.0, 1.0], [0.0, 0.0], [0.9, 0.9]])
        material, geometry = inputs(s, points)
        expected, _ = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        for mode in ("adaptive", "direct", "palette16", "palette32"):
            candidate = compact.compact(material, mode)
            self.assertIs(candidate.geometry, material.geometry)
            actual, _ = fragments.read(candidate, CAPTURE, points, [2, 2], geometry)
            for key in expected:
                np.testing.assert_array_equal(actual[key], expected[key])
        with self.assertRaises(rows.AttributeLimit):
            compact.compact(material, bytes_most=1)


if __name__ == "__main__":
    unittest.main()
