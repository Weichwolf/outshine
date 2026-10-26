"""Independent rectangle integration for metric frame/glass/storey-band fields."""

import unittest

import numpy as np
from outshine_ref.buildings.facade_finish import FacadeFinish
from outshine_ref.buildings.facades import FacadeRepeat


def rectangles(repeat, finish):
    frames, glasses, bands = [], [], []
    cell_w, cell_h = repeat.cell_size_m
    width, height = cell_w * repeat.opening_width, cell_h * repeat.opening_height
    for column in range(repeat.columns):
        for row in range(repeat.rows):
            x, y = (column + 0.5) * cell_w, (row + 0.5) * cell_h
            left, right = x - width / 2, x + width / 2
            bottom, top = y - height / 2, y + height / 2
            if width == 0 or height == 0:
                continue
            inset = finish.frame_m
            if 2 * inset >= min(width, height):
                frames.append((left, bottom, right, top))
            else:
                glasses.append(
                    (left + inset, bottom + inset, right - inset, top - inset)
                )
                frames.extend(
                    [
                        (left, bottom, left + inset, top),
                        (right - inset, bottom, right, top),
                        (left + inset, bottom, right - inset, bottom + inset),
                        (left + inset, top - inset, right - inset, top),
                    ]
                )
    band_height = min(finish.band_m, cell_h - height)
    for row in range(repeat.rows + 1):
        y = row * cell_h
        bands.append(
            (
                0.0,
                max(0.0, y - band_height / 2),
                repeat.width_m,
                min(repeat.height_m, y + band_height / 2),
            )
        )
    return frames, glasses, bands


def reference(repeat, finish, centres, footprints):
    low, high = centres - footprints / 2, centres + footprints / 2
    fields = []
    for group in rectangles(repeat, finish):
        if not group:
            fields.append(np.zeros(len(centres)))
            continue
        group = np.asarray(group)
        sizes = np.maximum(
            np.minimum(high[:, None], group[None, :, 2:])
            - np.maximum(low[:, None], group[None, :, :2]),
            0.0,
        )
        fields.append(
            np.prod(sizes, axis=-1).sum(axis=-1) / np.prod(footprints, axis=-1)
        )
    return np.column_stack((1.0 - np.sum(fields, axis=0), *fields))


class FacadeFinishControls(unittest.TestCase):
    def test_closed_filter_matches_explicit_disjoint_rectangle_integration(self):
        rng = np.random.default_rng(245)
        for counts in ((1, 1), (4, 3), (13, 7)):
            for fractions in ((0.48, 0.53), (0.68, 0.65), (0.0, 0.0), (1.0, 1.0)):
                repeat = FacadeRepeat(47.3, 19.4, *counts, *fractions)
                for finish in (FacadeFinish(), FacadeFinish(0, 0), FacadeFinish(8, 8)):
                    centres = rng.uniform([-4, -4], [51, 23], (96, 2))
                    footprints = rng.uniform(0.001, 60, (96, 2))
                    actual = finish.coverage(repeat, centres, footprints)
                    expected = reference(repeat, finish, centres, footprints)
                    np.testing.assert_allclose(actual, expected, atol=2e-11, rtol=1e-10)
                    np.testing.assert_allclose(actual.sum(axis=-1), 1.0, atol=2e-16)
                    self.assertTrue(np.all(actual >= 0))

    def test_metric_mean_and_finite_edge_bands(self):
        repeat = FacadeRepeat(16.0, 9.0, 4, 3, 0.5, 0.6)
        finish = FacadeFinish(0.1, 0.2)
        actual = finish.coverage(repeat, [8, 4.5], [16, 9])
        opening, glass, band = 0.5 * 0.6, 0.45 * (0.6 - 0.2 / 3), 0.2 / 3
        np.testing.assert_allclose(
            actual, [1 - opening - band, opening - glass, glass, band], atol=1e-15
        )
        centres = np.array([[2, 1.5], [1.05, 1.5], [2, 0], [-2, -2], [16, 9]])
        footprints = np.full((5, 2), 0.01)
        np.testing.assert_allclose(
            finish.coverage(repeat, centres, footprints),
            reference(repeat, finish, centres, footprints),
            atol=1e-12,
        )

    def test_large_repeat_uses_same_local_material_without_expansion(self):
        repeat = FacadeRepeat(3.2e6, 2.9e6, 1000000, 1000000, 0.48, 0.53)
        local = FacadeRepeat(3.2, 2.9, 1, 1, 0.48, 0.53)
        centres = np.array([[0.25, 0.5], [0.5, 0.5], [0.75, 0.5]]) * local.cell_size_m
        footprints = np.full((3, 2), 0.1)
        finish = FacadeFinish()
        shifted = centres + np.array([800000, 700000]) * local.cell_size_m
        np.testing.assert_allclose(
            finish.coverage(repeat, shifted, footprints),
            finish.coverage(local, centres, footprints),
            atol=2e-8,
        )

    def test_invalid_widths_or_pixel_boxes_are_rejected(self):
        for value in (-0.01, np.nan, np.inf, True):
            with self.assertRaises(ValueError):
                FacadeFinish(value, 0.1)
            with self.assertRaises(ValueError):
                FacadeFinish(0.1, value)
        repeat = FacadeRepeat(8, 9, 2, 3, 0.5, 0.6)
        for footprints in ([0, 1], [-1, 1], [np.nan, 1]):
            with self.assertRaises(ValueError):
                FacadeFinish().coverage(repeat, [2, 2], footprints)


if __name__ == "__main__":
    unittest.main()
