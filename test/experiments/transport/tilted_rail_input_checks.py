"""Original camera receipt and explicit frozen file-pin graph reader compatibility."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from rail_graph_inputs import read_window


class GraphReceipts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        folder = self.root / "results/control/components"
        folder.mkdir(parents=True)
        (self.root / "inputs").mkdir()
        (self.root / "inputs/control-input.json").write_text(
            json.dumps(
                {
                    "roads": [
                        {"source_id": "original-a", "properties": {"class": "rail"}}
                    ]
                }
            )
        )
        (folder / "source-usage.json").write_text(
            json.dumps([{"id": "original-a", "status": "used"}])
        )
        (folder / "source-ids.json").write_text(json.dumps(["original-a"]))
        (folder / "domain.wkb").write_bytes(shapely.box(0, -1.9, 10, 1.9).wkb)
        np.savez_compressed(
            folder / "graph.npz",
            **{
                "0_vertices": [[0.0, 0], [10, 0]],
                "0_edges": [[0, 1]],
                "0_half_widths": [1.9],
                "0_line_offsets": [0, 2],
                "0_line_coordinates": [[0.0, 0], [10, 0]],
                "0_owner_offsets": [0, 1],
                "0_owner_sources": [0],
            },
        )
        self.files = {
            str(p.relative_to(self.root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in self.root.rglob("*")
            if p.is_file()
        }
        (self.root / "generation.json").write_text(json.dumps({"files": self.files}))

    def tearDown(self):
        self.temp.cleanup()

    def test_default_and_explicit_pins_keep_identical_graph_and_sources(self):
        old = read_window(self.root, "control")
        explicit = read_window(self.root, "control", files=self.files)
        np.testing.assert_array_equal(old[2][0]["edges"], explicit[2][0]["edges"])
        self.assertEqual(old[2][0]["sources"], [("original-a",)])
        self.assertTrue(old[3].equals(explicit[3]))
        self.assertIsNotNone(old[4]["generation_sha256"])
        self.assertIsNone(explicit[4]["generation_sha256"])
        self.assertEqual(old[4]["input_sha256"], explicit[4]["input_sha256"])

    def test_both_reader_forms_reject_changed_source_bytes(self):
        (self.root / "inputs/control-input.json").write_text("{}")
        for options in ({}, {"files": self.files}):
            with self.assertRaisesRegex(ValueError, "captured rail input changed"):
                read_window(self.root, "control", **options)


if __name__ == "__main__":
    unittest.main()
