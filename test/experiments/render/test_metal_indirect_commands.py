import tempfile
import unittest
from pathlib import Path

import numpy as np
from metal_indirect_commands import prepare


class CommandInputs(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-indirect-inputs-")
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.output = self.root / "prepared"
        self.jobs = np.array([[0, 0, 0, 3], [1, 0, 3, 3], [2, 1, 6, 3]], dtype="<u4")
        self.args = np.array([[3, 1, 5, 0, 0], [3, 1, 0, 0, 1]], dtype="<u4")
        self.indices = np.array([0, 1, 2, 2, 1, 3, 3, 2, 0], dtype="<u4")
        self.write()

    def tearDown(self):
        self.temp.cleanup()

    def write(self):
        self.jobs.tofile(self.source / "jobs.bin")
        self.args.tofile(self.source / "arguments.bin")
        self.indices.tofile(self.source / "indices.bin")
        np.array([0, 3, 3], dtype="<u4").tofile(self.source / "kept.bin")
        np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1], [1, 1, 1]], dtype="<f4").tofile(
            self.source / "vertices.bin"
        )
        placements = np.zeros((2, 36), dtype="<f4")
        placements[:, :16] = np.eye(4, dtype="<f4").ravel()
        placements.tofile(self.source / "placements.bin")
        view = np.zeros(64, dtype="<f4")
        view[32:48] = np.eye(4, dtype="<f4").ravel()
        view.tofile(self.source / "uniform.bin")

    def test_preserves_compacted_holes_rows_and_original_indices(self):
        report = prepare(self.source, self.output)
        self.assertEqual(
            (report["rows"], report["nonempty_rows"], report["triangles"]), (2, 2, 2)
        )
        actual = np.fromfile(self.output / "draw-indices.bin", dtype="<u4")
        np.testing.assert_array_equal(actual, [3, 2, 0, 0, 0, 2, 1, 3])
        self.assertEqual(
            (self.output / "arguments.bin").read_bytes(), self.args.tobytes()
        )

    def test_count_disagreement_is_rejected_before_native_access(self):
        self.args[0, 0] = 6
        self.write()
        with self.assertRaisesRegex(ValueError, "argument counts"):
            prepare(self.source, self.output)

    def test_signed_base_vertices_are_preserved(self):
        self.args[:, 3] = np.array([-4, -4], dtype="<i4").view("<u4")
        self.indices += 4
        self.write()
        prepare(self.source, self.output)
        self.assertEqual(
            (self.output / "arguments.bin").read_bytes(), self.args.tobytes()
        )

    def test_invalid_signed_base_vertex_is_rejected(self):
        self.args[:, 3] = np.array([-5, -5], dtype="<i4").view("<u4")
        self.write()
        with self.assertRaisesRegex(ValueError, "invalid vertex reference"):
            prepare(self.source, self.output)


if __name__ == "__main__":
    unittest.main()
