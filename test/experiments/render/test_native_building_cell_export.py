"""Exercise the native reference boundary without network, GPU or shared-cache writes."""

import argparse
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

from check_building_full_view import check


class NativeCellExport(unittest.TestCase):
    exporter = None

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="outshine-building-cell-test-"
        )
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        data = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view"
        )
        with tarfile.open(data / "original-inputs.tar.gz") as archive:
            for member in archive.getmembers():
                path = (self.root / member.name).resolve()
                self.assertTrue(member.isfile() and self.root in path.parents)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.extractfile(member).read())
        self.input = next(self.root.glob("*/input.json"))
        self.metadata = json.loads(self.input.read_text())
        self.output = self.root / "output.json"

    def run_export(self, *args, code=0):
        result = subprocess.run(
            [str(self.exporter), str(self.input), str(self.output), *args],
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, code, result.stderr)
        return json.loads(self.output.read_text()) if code == 0 else None

    def test_new_control_matches_the_independent_archived_cell(self):
        archived = self.run_export("--full-view")
        original = (self.input.parent / self.metadata["view_file"]).read_bytes()
        self.assertEqual(check(archived, original)["sources"], 576)
        self.output.unlink()
        current = self.run_export("--full-cell", "33")
        self.assertEqual(current["sources"], archived["sources"])
        self.assertEqual(
            current["results"][0]["geometry"], archived["results"][0]["geometry"]
        )
        self.assertEqual(
            current["results"][0]["details"], archived["results"][0]["details"]
        )
        self.assertNotIn("archived_geometry_identical", current["results"][0])

    def test_invalid_cells_arguments_and_existing_output_are_rejected(self):
        for args in [
            ("--full-cell",),
            ("--full-cell", "0"),
            ("--full-cell", "-1"),
            ("--full-cell", "33x"),
            ("--full-cell", "4294967296"),
            ("--full-cell", "999"),
            ("--unknown",),
            ("--captures", "0"),
        ]:
            with self.subTest(args=args):
                self.run_export(*args, code=2)
                self.assertFalse(self.output.exists())
        self.output.write_bytes(b"retained original output")
        self.run_export("--full-cell", "33", code=2)
        self.assertEqual(self.output.read_bytes(), b"retained original output")

    def test_malformed_missing_duplicate_and_corrupted_inputs_are_rejected(self):
        damaged = [[], {}, self.metadata | {"cluster_triangles": -1}]
        damaged += [self.metadata | {"forms": self.metadata["forms"][:-1]}]
        damaged += [self.metadata | {"forms": self.metadata["forms"] * 2}]
        for document in damaged:
            with self.subTest(document_type=type(document).__name__):
                self.input.write_text(json.dumps(document))
                self.run_export("--full-cell", "33", code=2)
                self.assertFalse(self.output.exists())
        self.input.write_text(json.dumps(self.metadata))
        view = self.input.parent / self.metadata["view_file"]
        payload = bytearray(view.read_bytes())
        payload[-1] ^= 1
        view.write_bytes(payload)
        self.run_export("--full-view", code=2)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, required=True)
    args, remaining = parser.parse_known_args()
    NativeCellExport.exporter = args.exporter.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
