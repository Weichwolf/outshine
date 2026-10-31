import unittest
from unittest.mock import patch

import numpy as np
from source_domain_prepared_view import select_sources
from source_domain_reconstruct_study import checked_selection


class SourceSelectionTest(unittest.TestCase):
    def test_whole_sources_and_unknown_bounds(self):
        bounds = np.array(
            [
                [[-0.5, -0.5, 0], [0.5, 0.5, 0]],
                [[2, 0, 0], [3, 1, 0]],
                [[np.nan, 0, 0], [3, 1, 0]],
                [[2, 0, 0], [3, 1, 0]],
            ],
            dtype="<f4",
        )
        prepared = {
            "source_ids": np.array([11, 12, 13, 14]),
            "bounds": bounds,
            "normal": np.array([True, True, True, False]),
        }
        operands = (np.eye(4), np.zeros(4), np.eye(4))
        with patch("source_domain_prepared_view.frame", return_value=operands):
            self.assertEqual(select_sources(prepared, None, None), [11, 13, 14])
        with patch("source_domain_prepared_view.frame", return_value=None):
            self.assertEqual(select_sources(prepared, None, None), [11, 12, 13, 14])

    def test_changed_or_incomplete_selection_rejected(self):
        expected = [11, 13, 14]
        self.assertEqual(
            checked_selection(
                {"selected_sources": 3, "selected_source_ids": expected}, expected
            ),
            set(expected),
        )
        for ids, count in (
            ([11, 13], 2),
            ([11, 13, 14, 14], 4),
            ([11, 13, 14, 15], 4),
            (expected, 2),
        ):
            with self.subTest(ids=ids, count=count), self.assertRaises(ValueError):
                checked_selection(
                    {"selected_sources": count, "selected_source_ids": ids}, expected
                )


if __name__ == "__main__":
    unittest.main()
