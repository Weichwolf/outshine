"""Check native control coordinates and holes without regenerating source geometry."""

import copy
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from export_urban_patches import control_field
from urban_patches import UrbanPatches


def example():
    return UrbanPatches(
        np.array([[-2, 3], [0, 3]], np.int32),
        np.array([[[1, 0], [2, 3]], [[0, 3], [1, 2]]], np.uint8),
        np.array([0, 6, 12, 24], float),
        np.array([160, -80, 0.5, 16, 0.75]),
        np.zeros((2, 2, 3), np.float32),
        np.array([0, 1, 2], np.uint32),
        np.array([0, 1], np.uint32),
    )


class NativeInputTest(unittest.TestCase):
    def test_shared_source_control_input_and_original_snapshot_pins(self):
        fixtures = Path(__file__).parent / "fixtures"
        field = (fixtures / "coarse_city_native.field").read_bytes()
        reference = json.loads(
            (fixtures / "coarse_city_native.reference.json").read_text()
        )
        self.assertEqual(
            np.linalg.det(np.array(reference["native_to_reference"])[:3, :3]), 1
        )
        self.assertEqual(
            hashlib.sha256(field).hexdigest(), reference["input_field_sha256"]
        )
        snapshot = (fixtures / "coarse_city_source.json.gz").read_bytes()
        self.assertEqual(
            hashlib.sha256(snapshot).hexdigest(), reference["snapshot_sha256"]
        )
        words = field.split()
        columns, rows = map(int, words[:2])
        self.assertEqual([columns, rows], reference["grid_columns_rows"])
        grid = np.array(words[4:], np.float32).reshape(rows, columns)
        self.assertEqual(np.count_nonzero(grid), reference["occupied_cells"])
        self.assertEqual(len(words), 4 + columns * rows)

    def test_explicit_grid_holes_and_y_up_geometry_preserve_the_source_cells(self):
        product = example()
        text, report = control_field(product, [1, 0])
        columns, rows, pitch, fill, *tops = map(float, text.split())
        self.assertEqual((columns, rows, pitch, fill), (6, 2, 8, 0.75))
        np.testing.assert_array_equal(
            np.array(tops).reshape(2, 6), [[12, 24, 0, 0, 6, 12], [6, 0, 0, 0, 0, 24]]
        )
        self.assertEqual(report["occupied_cells"], 6)
        self.assertEqual(report["global_first_cell_xy"], [-4, 6])
        self.assertEqual(report["native_first_reference_cell_xy"], [-4, 7])
        transform = np.array(report["native_to_reference"])
        self.assertEqual(np.linalg.det(transform[:3, :3]), 1)
        np.testing.assert_array_equal(transform @ [0, 0, 0, 1], [-112, 104, 0, 1])
        np.testing.assert_array_equal(transform[:3, :3] @ [0, 0, 1], [0, -1, 0])
        # Independent global-cell coordinates versus the documented native box template.
        grid = np.array(tops).reshape(2, 6)
        for row, column in np.argwhere(grid > 0):
            height = grid[row, column]
            native_low = [(column + 0.125) * 8, 0, (row + 0.125) * 8, 1]
            native_high = [(column + 0.875) * 8, height, (row + 0.875) * 8, 1]
            cell = np.array([column - 4, 7 - row])
            centre = ((cell + 0.5) * 16 - [160, -80]) * 0.5
            np.testing.assert_array_equal(
                transform @ native_low, [centre[0] - 3, centre[1] + 3, 0, 1]
            )
            np.testing.assert_array_equal(
                transform @ native_high, [centre[0] + 3, centre[1] - 3, height, 1]
            )
        self.assertEqual(text, control_field(product, [0, 1])[0])

    def test_unsupported_native_limits_or_duplicate_cells_never_truncate(self):
        product = example()
        for chunks in [[], [0, 0], [2], [-1], [0.5], [True]]:
            with self.subTest(chunks=chunks), self.assertRaises(ValueError):
                control_field(product, chunks)
        for key in [[126, 3], [-2, 3]]:
            bad = copy.deepcopy(product)
            bad.chunks[1] = key
            with self.subTest(key=key), self.assertRaises(ValueError):
                control_field(bad, [0, 1])
        for height in [0.1, 1025, 1e300]:
            bad = copy.deepcopy(product)
            bad.heights[1] = height
            with self.subTest(height=height), self.assertRaises(ValueError):
                control_field(bad, [0, 1])

    def test_empty_selected_cells_are_only_a_diagnostic_subset(self):
        product = example()
        product.tiers[:] = 0
        text, report = control_field(product, [0, 1])
        self.assertEqual(set(map(float, text.split()[4:])), {0})
        self.assertEqual(report["occupied_cells"], 0)
        self.assertIn("never evidence of World Empty", report["scope"])

    def test_fresh_ready_export_does_not_import_source_or_geos(self):
        code = """
import builtins, sys
original = builtins.__import__
def checked(name, *args, **kwargs):
    if name.split('.')[0] in {'shapely', 'mapbox_vector_tile', 'coarse_city_domains', 'urban_patch_bake'}:
        raise AssertionError(name)
    return original(name, *args, **kwargs)
builtins.__import__ = checked
from export_urban_patches import control_field
from urban_patches import load
text, report = control_field(load(sys.argv[1]), [0, 1])
assert report['occupied_cells'] == 6
"""
        with tempfile.TemporaryDirectory(
            prefix="outshine-urban-native-input-test-"
        ) as folder:
            path = Path(folder) / "product.npz"
            np.savez_compressed(path, **example().arrays())
            subprocess.run(
                [sys.executable, "-B", "-W", "error", "-c", code, str(path)], check=True
            )


if __name__ == "__main__":
    unittest.main()
