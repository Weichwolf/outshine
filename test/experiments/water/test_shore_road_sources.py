import gzip
import json
import tempfile
import unittest
from pathlib import Path

from shore_road_sources import bind, interfaces


class PrimitiveSourceBinding(unittest.TestCase):
    def test_face_ordinals_use_index_words_and_junction_membership_stays_shared(self):
        ranges = [
            {
                "part": [0, 4],
                "source": 7,
                "source_domain": "lane",
                "index_word_range": [0, 3],
            },
            {
                "part": [1, 8],
                "source": 48,
                "source_domain": "junction_node",
                "index_word_range": [3, 6],
            },
        ]
        inputs = {
            "road-primitive-ranges.json.gz": {
                "pages": [{"render_part": 92, "page_key": "page", "ranges": ranges}]
            },
            "road-bindings.json.gz": {
                "plan_bindings": [
                    {"part": [0, 4], "lane_contacts": [[7, 100]]},
                    {"part": [1, 8], "lane_contacts": [[7, 100], [9, 200]]},
                ]
            },
            "roads.json.gz": {
                "source_ways": [{"ordinal": 7, "provider_feature_id": 12345}]
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, data in inputs.items():
                (root / name).write_bytes(
                    gzip.compress(json.dumps(dict(data, plan_key="original")).encode())
                )
            rows = bind(root, [[92, 0], [92, 1]], "original")
            self.assertEqual(rows[0]["primitive"]["source"], 7)
            self.assertEqual(rows[1]["primitive"]["source_domain"], "junction_node")
            self.assertEqual(rows[1]["lane_contacts"], [[7, 100], [9, 200]])
            self.assertIsNone(rows[1]["source_ways"][1])
            compact = interfaces(root, [[[92, 0]], [[92, 0], [92, 1]]], "original")
            self.assertEqual(compact["node_faces"], [[0], [0, 1]])
            self.assertEqual(len(compact["source_ways"]), 1)
            for pair, key in (
                ([92, 3], "original"),
                ([91, 0], "original"),
                ([92, 0], "other-plan"),
            ):
                with self.assertRaises(ValueError):
                    bind(root, [pair], key)


if __name__ == "__main__":
    unittest.main()
