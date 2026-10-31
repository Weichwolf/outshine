"""Metadata selection must defer all group/fallback payload validation and IO."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from source_domain_prepared_view import run


class MetadataSelectionTest(unittest.TestCase):
    def test_demand_includes_whole_fallback_without_opening_absent_payloads(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            index = np.array(
                [(42, 0, 100)],
                dtype=[("source", "<i4"), ("offset", "<u8"), ("bytes", "<u4")],
            )
            np.save(root / "index.npy", index)
            bound = root / "bounds.npz"
            np.savez(
                bound,
                index_digest=np.frombuffer(
                    hashlib.sha256((root / "index.npy").read_bytes()).digest(), "u1"
                ),
                source_ids=np.array([42, 99]),
                bounds=np.zeros((2, 2, 3)),
                normal=np.zeros((2, 3)),
                group_offsets=np.array([0]),
                group_lengths=np.array([100]),
                group_digests=np.zeros((1, 32), "u1"),
                fallback_digest=np.zeros(32, "u1"),
            )
            np.savez(
                root / "native-visible-triangles.npz",
                **{
                    "near-direction/view_words": np.zeros(1),
                    "placements/words": np.zeros(1),
                },
            )
            args = SimpleNamespace(
                bounds=bound,
                bounds_sha256=hashlib.sha256(bound.read_bytes()).hexdigest(),
                chunk=root,
                fallback=root / "absent-fallback.npz",
                angle=65,
                output=root / "selected.json",
            )
            with (
                patch("source_domain_prepared_view.DATA", root),
                patch(
                    "source_domain_prepared_view.read_controls",
                    return_value=({65: None}, None),
                ),
                patch(
                    "source_domain_prepared_view.select_sources", return_value=[42, 99]
                ),
                patch(
                    "source_domain_prepared_view.decode_raw", side_effect=AssertionError
                ),
            ):
                retained, identities = run(args, retain_angle=65, metadata_only=True)
                self.assertEqual((retained, identities), ({}, (42, 99)))
                result = json.loads(args.output.read_text())
                self.assertTrue(result["payload_validation_deferred"])
                row = result["cases"][0]
                self.assertEqual(row["selected_source_ids"], [42, 99])
                self.assertEqual(row["selected_original_fallback_sources"], 1)
                self.assertEqual((row["group_bytes"], row["fallback_bytes"]), (0, 0))
                with self.assertRaises(FileNotFoundError):
                    run(args, retain_angle=65)


if __name__ == "__main__":
    unittest.main()
