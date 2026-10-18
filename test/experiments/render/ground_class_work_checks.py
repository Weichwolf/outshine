"""Independent packed-grid examples for the classifier work inventory."""

import tempfile
import unittest
from pathlib import Path

import numpy as np

from ground_class_work import analyse


def fixture():
    words = np.zeros(60, dtype="<u4")
    words[0] = 4
    words[4:6] = (3, 1)
    words[8] = np.float32(4).view("u4")
    words[9:13] = (28, 34, 46, 56)
    words[28:34] = (2 << 16 | 1, 0, 2, 123456, 1 << 16 | 255, 2)
    words[34:46] = (1, 0, 3, 0, 2, 3, 5, 0, 3, 8, 2, 0)
    return words


class ClassWork(unittest.TestCase):
    def analyse_words(self, words):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "classes.u32"
            path.write_bytes(words.tobytes())
            return analyse(path)

    def test_independent_cell_ranges(self):
        result = self.analyse_words(fixture())
        fine = result["fine"]
        self.assertEqual(fine["reference_visits_per_cell"]["total"], 8 + 0 + 2)
        self.assertEqual(fine["reference_visits_per_cell"]["max"], 8)
        self.assertEqual(fine["reference_visits_per_cell"]["mean"], 10 / 3)
        self.assertEqual(fine["seeds_per_cell"]["total"], 2 + 0 + 1)
        self.assertEqual(fine["direct_class_cells"], 2)
        self.assertEqual(fine["reference_visits_per_cell"]["bins"]["0..0"], 1)
        self.assertIsNone(result["coarse"])

    def test_unreachable_seed_does_not_add_work_or_reject_the_grid(self):
        words = fixture()
        words[28] = 1
        words[35] = 123456
        result = self.analyse_words(words)
        self.assertEqual(result["fine"]["reference_visits_per_cell"]["total"], 2)

    def test_corrupt_reachable_ranges_are_rejected(self):
        for index, value in ((0, 55), (9, 100), (29, 5), (35, 123456), (36, 100)):
            with self.subTest(index=index):
                words = fixture()
                words[index] = value
                with self.assertRaises(ValueError):
                    self.analyse_words(words)
        with self.assertRaises(ValueError):
            self.analyse_words(fixture()[:2])


if __name__ == "__main__":
    unittest.main()
