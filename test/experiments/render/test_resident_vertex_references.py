import unittest

import numpy as np

from resident_vertex_references import measure


def fixture():
    roots = np.zeros((3, 12), dtype=np.uint32)
    roots[:, 4] = [0, 1, 1]
    roots[:, 5] = 1
    roots[:, 6] = [0, 1, 2]
    roots[:, 7] = [0, 1, 1]
    batches = np.zeros((2, 12), dtype=np.uint32)
    batches[:, 0] = [7, 8]
    batches[:, 5] = 1
    batches[:, 10] = [1, 2]
    arguments = np.zeros((2, 5), dtype=np.uint32)
    arguments[:, 0] = [3, 6]
    return {"scope": "fixture", "generation": 1}, {
        "roots": roots,
        "templates": np.array([[0, 0, 3, 0], [0, 3, 3, 0]], dtype=np.uint32),
        "batches": batches, "source_indices": np.arange(6, dtype=np.uint32),
        "kept": np.array([3, 3, 3], dtype=np.uint32),
        "visible": np.ones(3, dtype=np.uint32), "arguments": arguments}


class StateCosts(unittest.TestCase):
    def test_shared_batch_roots_are_counted_once_each(self):
        result = measure(*fixture())
        first, second = result["state_costs"]
        self.assertEqual((first["material"], first["batches"], first["roots"]), (8, 1, 2))
        self.assertEqual(first["main_selected_indices"], 6)
        self.assertEqual(second["main_selected_indices"], 3)
        self.assertEqual(sum(row["main_selected_indices"] for row in result["state_costs"]),
                         result["selected_indices"])

    def test_invisible_and_shadow_roots_ignore_stale_kept_values(self):
        meta, streams = fixture()
        streams["visible"][1] = 0
        streams["roots"][2, 11] = 4
        streams["arguments"][1, 0] = 0
        result = measure(meta, streams)
        hidden = result["state_costs"][1]
        self.assertEqual(hidden["root_input_indices"], 6)
        self.assertEqual(hidden["visible_main_roots"], 0)
        self.assertEqual(hidden["main_selected_indices"], 0)

    def test_native_selection_disagreement_is_rejected(self):
        meta, streams = fixture()
        streams["arguments"][1, 0] = 3
        with self.assertRaises(ValueError):
            measure(meta, streams)


if __name__ == "__main__":
    unittest.main()
