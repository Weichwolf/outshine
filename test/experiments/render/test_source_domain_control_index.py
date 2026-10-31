"""Prepared control occurrence counts must bind whole sources and actual payloads."""

import hashlib
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_domain_chunk_study import encode_raw
from source_domain_control_index import load, prepare


class ControlIndexTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.group = encode_raw(
            {
                "source_ids": np.array([42, 43], dtype="<i4"),
                "source_patch_offsets": np.array([0, 1, 2], dtype="<u4"),
                "patch_polygon_offsets": np.array([0, 1, 2], dtype="<u4"),
                "polygon_ring_offsets": np.array([0, 1, 3], dtype="<u4"),
                "ring_vertex_offsets": np.array([0, 4, 8, 12], dtype="<u4"),
            }
        )
        (self.path / "groups.bin").write_bytes(self.group)
        index = np.array(
            [(43, 0, len(self.group)), (42, 0, len(self.group))],
            dtype=[("source", "<i4"), ("offset", "<u8"), ("bytes", "<u4")],
        )
        np.save(self.path / "index.npy", index)
        self.index_raw = (self.path / "index.npy").read_bytes()
        self.bounds = self.path / "bounds.npz"
        np.savez(
            self.bounds,
            index_digest=np.frombuffer(hashlib.sha256(self.index_raw).digest(), "u1"),
            group_offsets=np.array([0]),
            group_lengths=np.array([len(self.group)]),
            group_digests=np.array(
                [np.frombuffer(hashlib.sha256(self.group).digest(), "u1")]
            ),
        )
        self.output = self.path / "controls.npz"

    def test_counts_include_holes_and_follow_bound_index_order(self):
        prepare(self.bounds, self.path, self.output)
        digest = hashlib.sha256(self.output.read_bytes()).hexdigest()
        counts, size = load(
            self.output, digest, self.index_raw, self.bounds.read_bytes()
        )
        self.assertEqual(counts, {42: 4, 43: 8})
        self.assertEqual(size, self.output.stat().st_size)
        for expected, index, bounds in (
            ("wrong", self.index_raw, self.bounds.read_bytes()),
            (digest, self.index_raw, b"changed-bounds"),
        ):
            with self.assertRaises(ValueError):
                load(self.output, expected, index, bounds)

    def test_corrupt_group_is_rejected_before_preparation(self):
        (self.path / "groups.bin").write_bytes(self.group[:-1])
        with self.assertRaises(ValueError):
            prepare(self.bounds, self.path, self.output)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
