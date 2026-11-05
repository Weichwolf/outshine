"""Atomic grouping contracts; generated outputs stay outside the repository."""

import unittest

import numpy as np

from grouped_vertex_plan import plan


def fixture(kinds=(0, 0), motions=(1, 1), materials=(0, 0)):
    count = len(kinds)
    roots, batches = [], []
    for row, (kind, motion, material) in enumerate(zip(kinds, motions, materials)):
        roots.append([0, 0, 0, 0, 0, 1, row, row, row + 4, 2, 3, 0])
        batches.append([material, 4, kind, motion, 0, 1, 2, 3, row + 4, 0, 1, kind])
    return {
        "roots": np.array(roots, dtype=np.uint32),
        "batches": np.array(batches, dtype=np.uint32),
        "templates": np.array([[0, 0, 3, 0]], dtype=np.uint32),
        "source_indices": np.array([99, 99, 8, 4, 6], dtype=np.uint32).reshape(-1, 1),
        "visible": np.ones((count, 1), dtype=np.uint32),
        "kept": np.full((count, 1), 3, dtype=np.uint32),
        "arguments": np.array([[3, 1, row * 3, 0, row + 4] for row in range(count)],
                              dtype=np.uint32).reshape(-1, 1),
    }


class GroupedVertexPlanCases(unittest.TestCase):
    def test_shared_source_keeps_distinct_placements_and_virtual_addresses(self):
        result, shift = plan(fixture())
        self.assertEqual(len(result["groups"]), 1)
        self.assertEqual(shift, 2)
        self.assertEqual(result["references"].tolist(), [[2, 4], [2, 5]])
        self.assertEqual(result["indices"].tolist(), [0, 1, 2, 4, 5, 6])
        self.assertEqual(result["arguments"].tolist(), [[6, 1, 0, 0, 0]])

    def test_deforming_stream_is_grouped_separately_from_rigid_stream(self):
        result, _ = plan(fixture(motions=(0, 1)))
        self.assertEqual(len(result["groups"]), 2)
        self.assertEqual(result["arguments"][:, 2].tolist(), [0, 3])

    def test_blending_is_an_order_barrier(self):
        result, _ = plan(fixture(kinds=(0, 2, 0), motions=(1, 1, 1), materials=(0, 0, 0)))
        self.assertEqual(len(result["groups"]), 3)
        self.assertEqual(result["groups"][:, 1].tolist(), [1, 0, 1])
        self.assertEqual(result["indices"][3:6].tolist(), [8, 4, 6])

    def test_hidden_root_cannot_contribute_stale_kept_values(self):
        inputs = fixture()
        inputs["visible"][0] = 0
        inputs["kept"][0] = 99
        inputs["arguments"][0] = 0
        result, _ = plan(inputs)
        self.assertEqual(result["arguments"][0, 0], 3)
        self.assertEqual(result["indices"].tolist(), [4, 5, 6])

    def test_missing_geometry_is_rejected_by_native_count_comparison(self):
        inputs = fixture()
        inputs["arguments"][0] = 6
        with self.assertRaisesRegex(ValueError, "native draw arguments"):
            plan(inputs)


if __name__ == "__main__":
    unittest.main()
