"""Exact geometry binding and transparent native/CPU centre disagreements."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from building_sample_inputs import digest
from building_sample_originals import centre_control, read_original
from compare_building_sample_reuse import DATA


class OriginalSampleInputs(unittest.TestCase):
    def test_optional_attributes_keep_verified_original_parts_and_vertex_bindings(self):
        source, audit = read_original(DATA / "original-geometry", attributes=True)
        vertex, face = 0, 0
        payload = 0
        for part, attributes in zip(source["parts"], source["attributes"], strict=True):
            local = source["indices"][face : face + part["triangles"]] - vertex
            self.assertGreaterEqual(local.min(), 0)
            self.assertLess(local.max(), part["vertices"])
            self.assertIn("material", part)
            for name, width in (("normals", 3), ("uv", 2), ("uv1", 2), ("colours", 4)):
                self.assertEqual(attributes[name].shape, (part["vertices"], width))
                self.assertEqual(attributes[name].dtype, np.float32)
                payload += attributes[name].nbytes
            vertex += part["vertices"]
            face += part["triangles"]
        self.assertEqual((vertex, face), (audit["vertices"], audit["triangles"]))
        self.assertEqual(payload, vertex * 44)

    def test_all_native_geometry_operands_preserve_original_frame(self):
        source, audit = read_original(DATA / "original-geometry")
        self.assertEqual(audit["verified_files"], 19)
        self.assertEqual(audit["source_count"], 1558)
        self.assertEqual(audit["triangles"], 38476)
        self.assertEqual(audit["vertices"], 78374)
        self.assertEqual(source["positions"].dtype, np.dtype("<f4"))
        self.assertEqual(source["indices"].dtype, np.int64)

    def test_matching_npz_hash_does_not_rebind_modified_original_positions(self):
        actual = DATA / "original-geometry"
        with tempfile.TemporaryDirectory(prefix="outshine-original-samples-") as temp:
            parent = Path(temp)
            (parent / "manifest.json").write_bytes(
                (DATA / "manifest.json").read_bytes()
            )
            directory = parent / "original-geometry"
            directory.mkdir()
            manifest = json.loads((actual / "manifest.json").read_text())
            with np.load(actual / "geometry.npz", allow_pickle=False) as saved:
                arrays = dict(saved)
            arrays["0_positions_bin"][0] += 1
            np.savez_compressed(directory / "geometry.npz", **arrays)
            manifest.update(
                npz_sha256=digest(directory / "geometry.npz"),
                npz_bytes=(directory / "geometry.npz").stat().st_size,
            )
            (directory / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "operand changed"):
                read_original(directory)

    def test_centre_control_reports_mismatches_and_native_hits_outside_bounds(self):
        row = {
            "centre_forward_depth_m": [10, None, 20, None],
            "centre_origin": [1, 2],
            "centre_shape": [2, 2],
        }
        manifest = {"crop": [0, 0, 4, 4]}
        native = np.zeros((4, 4, 1), np.float32)
        native[2, 1, 0] = np.float32(0.1) / 10
        native[2, 2, 0] = np.float32(0.1) / 15
        native[0, 0, 0] = 0.01
        result = centre_control(row, native, manifest)
        self.assertEqual(result["coverage_mismatches"], 2)
        self.assertEqual(result["native_hits_outside_cpu_bounds"], 1)
        self.assertEqual(result["common_hits"], 1)
        self.assertLess(result["maximum_forward_depth_residual_m"], 1e-6)

    def test_outside_native_crop_cannot_be_silently_truncated(self):
        row = {
            "centre_forward_depth_m": [10],
            "centre_origin": [-1, 0],
            "centre_shape": [1, 1],
        }
        with self.assertRaisesRegex(ValueError, "outside"):
            centre_control(row, np.zeros((4, 4, 1)), {"crop": [0, 0, 4, 4]})


if __name__ == "__main__":
    unittest.main()
