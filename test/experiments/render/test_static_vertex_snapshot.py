import unittest

import numpy as np

from static_vertex_snapshot import measure


def fixture():
    roots = np.zeros((2, 12), dtype=np.uint32)
    roots[:, 5] = 1
    roots[:, 4] = [0, 1]
    roots[:, 7] = [0, 1]
    roots[:, 8] = [7, 2]
    templates = np.array([[0, 0, 3, 0], [0, 3, 3, 0]], dtype=np.uint32)
    batches = np.zeros((2, 12), dtype=np.uint32)
    batches[:, 5] = 1
    return {"scope": "fixture", "generation": 1}, {
        "roots": roots, "templates": templates, "batches": batches,
        "source_indices": np.array([5, 3, 4, 0, 2, 1], dtype=np.uint32)}


class SnapshotTests(unittest.TestCase):
    def test_actual_source_indices_define_model_ownership(self):
        result = measure(*fixture())
        self.assertEqual(result["merged_draws"], 2)
        self.assertEqual(result["compatible_groups"], 1)
        self.assertEqual(result["model_table_bytes"], 24)

    def test_shadow_roots_and_instanced_batches_do_not_enter_main_grouping(self):
        meta, data = fixture()
        data["roots"][0, 11] = 4
        data["batches"][1, 5] = 1000000
        result = measure(meta, data)
        self.assertEqual(result["merged_draws"], 0)
        self.assertEqual(result["fallback_draws"], 1)
        self.assertEqual(result["model_table_bytes"], 0)

    def test_foreign_template_or_index_ranges_fail(self):
        for stream, index, value in [("roots", (0, 4), 10),
                                     ("templates", (0, 1), 100)]:
            meta, data = fixture()
            data[stream][index] = value
            with self.assertRaises(ValueError):
                measure(meta, data)


if __name__ == "__main__":
    unittest.main()
