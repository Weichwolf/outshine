import copy
import io
import json
import tarfile
import tempfile
import unittest
from pathlib import Path

import numpy as np
from native_visibility_data import GROUP, digest, extract, partition
from native_visibility_pipeline import verify
from test_native_visibility_cost import scene


class NativeVisibilityPipeline(unittest.TestCase):
    def test_archive_rejects_missing_members_symlinks_and_changed_payload(self):
        for kind in ("valid", "missing", "symlink", "changed"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                reference = root / "payload"
                reference.write_bytes(b"native input")
                archive = root / "input.tar.xz"
                with tarfile.open(archive, "w:xz") as packed:
                    if kind != "missing":
                        member = tarfile.TarInfo("payload")
                        member.size = reference.stat().st_size
                        if kind == "symlink":
                            member.type = tarfile.SYMTYPE
                            member.linkname = "../outside"
                        data = b"changed data" if kind == "changed" else b"native input"
                        packed.addfile(member, io.BytesIO(data))
                (root / "manifest.json").write_text(
                    json.dumps(
                        {
                            "schema": 1,
                            "archive_sha256": digest(archive),
                            "canonical": {
                                "payload": {"bytes": 12, "sha256": digest(reference)}
                            },
                        }
                    )
                )
                if kind == "valid":
                    extract(archive, root / "unpacked")
                    self.assertEqual(
                        (root / "unpacked/payload").read_bytes(), b"native input"
                    )
                else:
                    with self.assertRaises(ValueError):
                        extract(archive, root / "unpacked")
                    self.assertFalse((root / "outside").exists())

    def test_batches_preserve_original_indices_and_reject_mixed_transforms(self):
        snapshot = scene()
        rows = np.zeros(2, dtype=GROUP)
        rows["info"] = [[0, 2, 0, 0], [1, 2, 6, 0]]
        order = np.array([0, 1])
        values = partition(snapshot, rows, order, np.array([0, 1]))
        np.testing.assert_array_equal(values[-1], snapshot.indices)
        snapshot.models[1, 0, 3] = 7
        with self.assertRaises(ValueError):
            partition(snapshot, rows, order, np.array([0, 0]))

    def test_missing_timings_false_rejections_and_repeated_variants_fail(self):
        valid = {
            "variants": [
                {
                    "label": f"{policy}-{limit}",
                    "verification": {
                        "false_rejections_vs_double": 0,
                        "original_indices_exact": True,
                        "baseline_depth_exact": True,
                    },
                    **{
                        mode: [1_000_000] * 12
                        for mode in (
                            "baseline_raster",
                            "select_compact",
                            "select_compact_raster",
                        )
                    },
                }
                for policy in ("source", "block")
                for limit in (0, 64, 32, 16, 8)
            ]
        }
        self.assertEqual(len(verify(valid)), 10)
        for kind in ("timing", "rejection", "indices", "depth", "variant"):
            with self.subTest(kind=kind):
                broken = copy.deepcopy(valid)
                row = broken["variants"][0]
                if kind == "timing":
                    row["select_compact"][0] = None
                elif kind == "rejection":
                    row["verification"]["false_rejections_vs_double"] = 1
                elif kind == "indices":
                    row["verification"]["original_indices_exact"] = False
                elif kind == "depth":
                    row["verification"]["baseline_depth_exact"] = False
                else:
                    row["label"] = "source-8"
                with self.assertRaises(ValueError):
                    verify(broken)


if __name__ == "__main__":
    unittest.main()
