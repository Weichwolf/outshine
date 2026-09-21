import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "test/harness/shared/corpus"))

from prep import jobs
from prep.refusal import Refusal


class Manifest:
    id = "fixture"


class SeedShift(unittest.TestCase):
    def test_equal_float_products_pass(self):
        rows = [
            {"recipe": "default", "digests": {"raw": "same"}},
            {"recipe": "seed-shift", "digests": {"raw": "same"}},
        ]
        self.assertIsNone(jobs._verify_seed_shift(Manifest(), rows))

    def test_changed_float_products_are_refused(self):
        rows = [
            {"recipe": "default", "frame": 2, "digests": {"raw": "first"}},
            {"recipe": "seed-shift", "frame": 2, "digests": {"raw": "second"}},
        ]
        with self.assertRaisesRegex(Refusal, "changing only the seed changed its float pixels"):
            jobs._verify_seed_shift(Manifest(), rows)


if __name__ == "__main__":
    unittest.main()
