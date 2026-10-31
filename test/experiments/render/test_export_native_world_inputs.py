"""A platform math library must not silently regenerate a bound camera."""

import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from export_native_world_field import CONTROLS, camera_inputs, queries
from native_world_inputs import DATA, digest, load


class CameraInput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controls = json.loads(CONTROLS.read_text())
        cls.directory = DATA / "ss2"
        cls.manifest, _, cls.cases, _ = load(cls.directory)

    def test_bound_values_reach_both_capture_and_pbr_without_regeneration(self):
        with patch(
            "export_native_world_field.world_cameras",
            side_effect=AssertionError("Camera regenerated"),
        ):
            cameras, capture, binding = camera_inputs(self.controls, 2, self.directory)
        self.assertEqual(capture, self.manifest["capture"])
        self.assertEqual(
            binding, digest((self.directory / "manifest.json").read_bytes())
        )
        for query in queries(cameras):
            actual = self.cases["original", query["name"]]
            self.assertEqual(query["capture"], query["lighting_camera"]["capture"])
            for name, value in query["capture"].items():
                self.assertEqual(value, actual["capture"][name])

    def test_profile_mismatch_is_refused_instead_of_rescaled(self):
        with self.assertRaisesRegex(ValueError, "requested raster/profile"):
            camera_inputs(self.controls, 1, self.directory)

    def test_camera_metadata_neither_reads_nor_admits_pixel_arrays(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.camera-header."
        ) as temporary:
            directory = Path(temporary)
            shutil.copyfile(
                self.directory / "manifest.json", directory / "manifest.json"
            )
            cameras, _, _ = camera_inputs(self.controls, 2, directory)
            self.assertEqual(cameras, self.manifest["world_cameras"])
            with self.assertRaises(FileNotFoundError):
                load(directory)

    def test_another_source_control_cannot_supply_camera_provenance(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.camera-source."
        ) as temporary:
            directory = Path(temporary)
            changed = copy.deepcopy(self.manifest)
            changed["controls_sha256"] = "0" * 64
            (directory / "manifest.json").write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "source/control binding"):
                camera_inputs(self.controls, 2, directory)

    def test_one_ulp_request_change_is_not_accepted_after_rehash(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.camera-input."
        ) as temporary:
            directory = Path(temporary)
            shutil.copyfile(self.directory / "samples.npz", directory / "samples.npz")
            changed = copy.deepcopy(self.manifest)
            changed["world_cameras"]["near-direction"]["yfov_rad"] = float(
                np.nextafter(
                    changed["world_cameras"]["near-direction"]["yfov_rad"], 0.0
                )
            )
            (directory / "manifest.json").write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "capture/raster differ"):
                camera_inputs(self.controls, 2, directory)

    def test_default_keeps_the_existing_analytic_camera_path(self):
        expected = {"near-direction": {"yfov_rad": 0.8}}
        with patch(
            "export_native_world_field.world_cameras", return_value=expected
        ) as derive:
            cameras, capture, binding = camera_inputs(self.controls, 1)
        derive.assert_called_once_with(self.controls, 1)
        self.assertIs(cameras, expected)
        self.assertEqual([capture[k] for k in ("width_px", "height_px")], [128, 128])
        self.assertIsNone(binding)


if __name__ == "__main__":
    unittest.main()
