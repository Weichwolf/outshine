import gzip
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from shore_bank_inputs import roads, sample_roads


class CapturedRoadInterfaces(unittest.TestCase):
    def test_column_major_placement_precedes_east_up_south_conversion(self):
        matrix = np.array(
            [[0, 0, 1, 100], [0, 1, 0, 200], [-1, 0, 0, 300], [0, 0, 0, 1]]
        )
        data = {
            "anchor": [10, 40],
            "plan_key": "original-plan",
            "parts": [
                {
                    "part": 92,
                    "placement": matrix.ravel(order="F").tolist(),
                    "positions": [0, 7, 0, 2, 7, 0, 0, 7, 3],
                    "triangles": [0, 1, 2],
                }
            ],
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "roads.json.gz").write_bytes(
                gzip.compress(json.dumps(data).encode())
            )
            triangles, ids, key = roads(root, [10, 40], [99, -301, 104, -297])
            np.testing.assert_array_equal(
                triangles[0], [[100, -300, 207], [100, -298, 207], [103, -300, 207]]
            )
            self.assertEqual(ids.tolist(), [[92, 0]])
            self.assertEqual(key, "original-plan")
            with self.assertRaises(ValueError):
                roads(root, [11, 40], [99, -301, 104, -297])

    def test_height_on_shared_edge_is_consistent_but_independent_levels_decline(self):
        faces = np.array(
            [
                [[0, 0, 10], [1, 0, 11], [0, 1, 12]],
                [[1, 0, 11], [1, 1, 13], [0, 1, 12]],
            ],
            float,
        )
        points, bindings = sample_roads(faces, [[0.5, 0.5]])
        self.assertEqual(points[0, 2], 11.5)
        self.assertEqual(set(bindings[0]), {0, 1})
        faces[1, :, 2] += 0.3
        with self.assertRaisesRegex(ValueError, "ownership"):
            sample_roads(faces, [[0.5, 0.5]])
        with self.assertRaises(ValueError):
            sample_roads(faces, [[2, 2]])


if __name__ == "__main__":
    unittest.main()
