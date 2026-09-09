import hashlib
from pathlib import Path
import tempfile
import unittest
from PIL import Image
from reference_store import pin, resolve


class ReferenceStoreTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.image = self.root / "input.png"
        Image.new("RGB", (3, 2), (12, 34, 56)).save(self.image)
        self.cache = self.root / "cache"
        self.record = pin(self.image, 7, 0.25, self.cache)

    def test_pin_preserves_bytes_and_frame_identity(self):
        self.assertEqual(self.record["sha256"], hashlib.sha256(self.image.read_bytes()).hexdigest())
        self.assertEqual((self.record["frame"], self.record["seconds"]), (7, 0.25))
        self.assertEqual(resolve(self.record, self.cache).read_bytes(), self.image.read_bytes())

    def test_cache_does_not_need_original_image(self):
        self.image.unlink()
        self.assertTrue(resolve(self.record, self.cache).is_file())

    def test_missing_reference_is_an_error(self):
        resolve(self.record, self.cache).unlink()
        with self.assertRaisesRegex(ValueError, "missing pinned"):
            resolve(self.record, self.cache)

    def test_corruption_is_an_error(self):
        resolve(self.record, self.cache).write_bytes(b"not the pinned image")
        with self.assertRaisesRegex(ValueError, "corrupt pinned"):
            resolve(self.record, self.cache)

    def test_dimension_mismatch_is_an_error(self):
        wrong = dict(self.record, widthPx=4)
        with self.assertRaisesRegex(ValueError, "dimensions"):
            resolve(wrong, self.cache)

    def test_invalid_hash_cannot_escape_cache(self):
        for digest in ("../input.png", "z" * 64, "a" * 63):
            with self.subTest(digest=digest), self.assertRaisesRegex(ValueError, "SHA-256"):
                resolve(dict(self.record, sha256=digest), self.cache)

    def test_resolving_does_not_rewrite_pins_or_cache(self):
        original = dict(self.record)
        path = resolve(self.record, self.cache)
        before = path.stat().st_mtime_ns
        resolve(self.record, self.cache)
        self.assertEqual(self.record, original)
        self.assertEqual(path.stat().st_mtime_ns, before)


if __name__ == "__main__":
    unittest.main()
