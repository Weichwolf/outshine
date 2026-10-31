import unittest
from unittest.mock import patch

import numpy as np
from source_clip_demand import outside_xy, outside_xy_batched
from source_domain_prepared_view import select_sources
from source_domain_reconstruct_study import checked_selection


class SourceSelectionTest(unittest.TestCase):
    def test_batched_matches_scalar_and_keeps_unknown(self):
        rng = np.random.default_rng(941)
        lo = rng.uniform(-4, 4, size=(1000, 3)).astype("<f4")
        hi = lo + rng.uniform(0, 1, size=(1000, 3)).astype("<f4")
        bounds = np.stack((lo, hi), axis=1)
        bounds[0, 0, 0] = np.nan
        bounds[1, 0, 0] = 5
        bounds[2] = np.finfo(np.float32).max
        normal = rng.integers(0, 2, size=len(bounds)).astype(bool)
        normal[:3] = True
        operands = (np.eye(4), np.zeros(4), np.eye(4))
        expected = []
        for box, flag in zip(bounds, normal, strict=True):
            value = outside_xy(box, operands) if flag else None
            expected.append(value is not None and np.any(value < 0))
        for size in (1, 7, 256, 2048):
            np.testing.assert_array_equal(
                outside_xy_batched(bounds, normal, operands, batch_size=size), expected
            )
        self.assertFalse(outside_xy_batched(bounds, normal, None).any())

    def test_near_clip_plane_matches_scalar(self):
        one = np.float32(1)
        xs = np.array(
            [np.nextafter(one, np.float32(0)), one, np.nextafter(one, np.float32(2))],
            dtype="<f4",
        )
        bounds = np.zeros((len(xs), 2, 3), dtype="<f4")
        bounds[:, :, 0] = xs[:, None]
        normal = np.ones(len(xs), dtype=bool)
        operands = (np.eye(4), np.zeros(4), np.eye(4))
        expected = [np.any(outside_xy(box, operands) < 0) for box in bounds]
        np.testing.assert_array_equal(
            outside_xy_batched(bounds, normal, operands), expected
        )

    def test_transformed_boxes_match_scalar(self):
        rng = np.random.default_rng(971)
        lo = rng.uniform(-1000, 1000, (400, 3)).astype("<f4")
        bounds = np.stack((lo, lo + rng.uniform(0, 20, (400, 3))), axis=1).astype("<f4")
        normal = np.ones(len(bounds), dtype=bool)
        for _ in range(4):
            operands = (
                rng.normal(size=(4, 4)),
                rng.normal(size=4),
                rng.normal(size=(4, 4)),
            )
            expected = [
                (value is not None and np.any(value < 0))
                for value in (outside_xy(box, operands) for box in bounds)
            ]
            np.testing.assert_array_equal(
                outside_xy_batched(bounds, normal, operands), expected
            )

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
