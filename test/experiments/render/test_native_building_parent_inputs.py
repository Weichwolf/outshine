"""Boundary controls for the fixed native source-parent probe."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from current_visibility import project
from native_building_parent_inputs import (
    frame,
    original_records,
    parent_records,
    write_case,
)


class NativeParentInputs(unittest.TestCase):
    def test_roof_and_wall_keep_directional_coverage_and_material_roles(self):
        product = {
            "positions": np.array(
                [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]], np.float32
            ),
            "indices": np.array([[0, 1, 2], [0, 3, 1]], np.uint32),
            "triangle_groups": np.zeros(2, np.uint32),
            "group_coverage": np.array([[0.25, 0.5, 0.75]], np.float32),
        }
        material = {
            "albedo": np.array([[[0.1, 0.2, 0.3], [0.4, 0.5, 0.6]]], np.float32),
            "roughness_weights": np.zeros((1, 2, 9), np.float32),
        }
        material["roughness_weights"][0, 0, 8] = 1
        material["roughness_weights"][0, 1, 2] = 1
        records = parent_records(product, material)
        np.testing.assert_array_equal(records[:, 2], [[0, 0, 1, 0.25], [0, 1, 0, 0.5]])
        np.testing.assert_array_equal(records[:, 3, :3], material["albedo"][0, [1, 0]])
        self.assertEqual(records[0, 5, 2], 1)
        self.assertEqual(records[1, 7, 0], 1)

    def test_original_two_states_and_ninth_lobe_survive_gpu_record_packing(self):
        original = {
            "positions": np.array([[3, 4, 5]], np.float32),
            "uv": np.array([[0, 2]], np.float32),
            "indices": np.zeros((1, 3), np.uint32),
        }
        fields = {
            "uv_gradient": np.array([[0, 0, 0.25]]),
            "normal": np.array([[1, 0, 0]]),
            "albedo": np.array([[[0.2, 0.3, 0.4], [0.5, 0.6, 0.7]]]),
            "weights": np.zeros((1, 2, 9)),
        }
        fields["weights"][0, 0, 0] = 1
        fields["weights"][0, 1, 8] = 1
        records = original_records(original, fields, np.array([0]))
        np.testing.assert_array_equal(records[0, 0], [3, 4, 5, 2])
        np.testing.assert_array_equal(records[0, 1], [0, 0, 0.25, 1])
        self.assertEqual(records[0, 5, 0], 1)
        self.assertEqual(records[0, 10, 0], 1)
        self.assertEqual(records[:, 5:8].sum(), 1)
        self.assertEqual(records[:, 8:11].sum(), 1)

    def test_crop_keeps_presentation_pixel_spacing_and_native_y_convention(self):
        query = {
            "eye": [0, 0, 0],
            "basis": np.eye(3).tolist(),
            "focal_px": 900,
            "resolution": [1280, 720],
        }
        positions = np.array([[-1, -1, 8], [1, 1, 8], [0.2, -0.3, 10]])
        box, uniform = frame(query, positions, np.linspace(0.1, 1, 9))
        clip = uniform[:16].reshape(4, 4).T
        projected = np.column_stack((positions, np.ones(len(positions)))) @ clip.T
        ndc = projected[:, :2] / projected[:, 3, None]
        pixels = box[0] + (ndc * [1, -1] + 1) * (box[1] - box[0]) / 2
        expected, _ = project(positions, (np.zeros(3), np.eye(3), 900, 1280, 720))
        np.testing.assert_allclose(pixels, expected[:, :2], rtol=0, atol=1e-5)

    def test_payload_size_accounts_for_all_uploaded_diagnostic_arrays(self):
        with tempfile.TemporaryDirectory() as temporary:
            row = write_case(
                Path(temporary) / "case",
                np.zeros((3, 3)),
                np.array([[0, 1, 2]]),
                np.zeros((1, 11, 4)),
                [[0, 3]],
                True,
            )
            self.assertEqual(row["bytes"], 3 * 3 * 4 + 3 * 4 + 11 * 4 * 4)
            self.assertEqual(row["triangles"], 1)


if __name__ == "__main__":
    unittest.main()
