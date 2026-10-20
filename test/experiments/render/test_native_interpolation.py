import copy
import gzip
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from native_interpolation import FIXTURE, materialize_inputs, validate_inputs


class NativeInterpolationInputs(unittest.TestCase):
    def encoded(self, root):
        source = root / "source"
        source.mkdir()
        data = b"fixed index bytes\x00\x01\x02"
        packed = gzip.compress(data, mtime=0)
        (source / "indices.bin.gz").write_bytes(packed)
        manifest = {"files": {"indices.bin": {
            "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "encoded": "indices.bin.gz", "encoded_bytes": len(packed),
            "encoded_sha256": hashlib.sha256(packed).hexdigest(), "compression": "gzip",
        }}}
        return source, manifest, data

    def test_original_fixed_inputs_remain_valid_without_decoding(self):
        manifest = json.loads((FIXTURE / "manifest.json").read_text())
        validate_inputs(FIXTURE, manifest)
        self.assertEqual(materialize_inputs(FIXTURE, manifest, Path("unused")), FIXTURE)

    def test_current_case_decodes_to_its_pinned_original_indices(self):
        source = FIXTURE / "tinted-backface"
        manifest = json.loads((source / "manifest.json").read_text())
        validate_inputs(source, manifest)
        with tempfile.TemporaryDirectory(prefix="outshine-engine-interpolation.") as folder:
            destination = materialize_inputs(source, manifest, Path(folder))
            indices = destination / "indices.bin"
            self.assertEqual(indices.stat().st_size, 73029 * 4)
            self.assertEqual(hashlib.sha256(indices.read_bytes()).hexdigest(),
                             manifest["files"]["indices.bin"]["sha256"])
            self.assertEqual((destination / "parameters.bin").read_bytes(),
                             (source / "parameters.bin").read_bytes())

    def test_changed_encoded_input_is_rejected_before_decode(self):
        with tempfile.TemporaryDirectory(prefix="outshine-engine-interpolation.") as folder:
            source, manifest, _ = self.encoded(Path(folder))
            (source / "indices.bin.gz").write_bytes(b"changed")
            with self.assertRaises(ValueError):
                validate_inputs(source, manifest)

    def test_encoded_hash_does_not_replace_decoded_hash(self):
        with tempfile.TemporaryDirectory(prefix="outshine-engine-interpolation.") as folder:
            source, manifest, _ = self.encoded(Path(folder))
            manifest["files"]["indices.bin"]["sha256"] = "0" * 64
            validate_inputs(source, manifest)
            with self.assertRaises(ValueError):
                materialize_inputs(source, manifest, Path(folder))

    def test_decoding_has_a_bound_and_rejects_truncated_payload(self):
        with tempfile.TemporaryDirectory(prefix="outshine-engine-interpolation.") as folder:
            root = Path(folder)
            source, manifest, data = self.encoded(root)
            oversized = copy.deepcopy(manifest)
            oversized["files"]["indices.bin"]["bytes"] = 1024 * 1024 + 1
            with self.assertRaises(ValueError):
                materialize_inputs(source, oversized, root)
            other = root / "other"
            other.mkdir()
            manifest["files"]["indices.bin"]["bytes"] = len(data) + 1
            with self.assertRaises(ValueError):
                materialize_inputs(source, manifest, other)

    def test_fixture_names_cannot_escape_the_owned_directory(self):
        with tempfile.TemporaryDirectory(prefix="outshine-engine-interpolation.") as folder:
            source, manifest, _ = self.encoded(Path(folder))
            for name in ("../outside", "/tmp/outside", "..", ""):
                altered = copy.deepcopy(manifest)
                altered["files"]["indices.bin"]["encoded"] = name
                with self.assertRaises(ValueError):
                    validate_inputs(source, altered)


if __name__ == "__main__":
    unittest.main()
