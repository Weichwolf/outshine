"""Native query bindings, signed-stage cancellation and actual fitted-plane control."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from building_plane_coverage import geometry_controls, signed_components
from building_sample_inputs import digest, readbacks
from building_sample_originals import read_original
from building_sample_source import read_query, read_source
from compare_building_sample_reuse import DATA
from compare_building_support_plane import analyse
from field_view_plane import FieldPlane
from field_view_sampling import FieldGrid, ImageView


class BuildingPlaneControls(unittest.TestCase):
    def test_real_depth_query_is_bound_to_the_existing_source_and_original(self):
        manifest, arrays = read_query(
            DATA / "samples-512", DATA / "source-512", DATA / "high-sampling"
        )
        self.assertEqual(set(arrays), {"linear", "depth", "normal", "ss4096"})
        self.assertEqual(manifest["raster"], [1280, 720])
        self.assertEqual(arrays["ss4096"].shape, (96, 128, 4))
        self.assertEqual(manifest["native_report"]["cases"][0]["triangles"], 86098)

    def test_depth_query_cannot_be_rebound_only_by_changing_container_checksum(self):
        actual = DATA / "samples-512"
        manifest = json.loads((actual / "manifest.json").read_text())
        with np.load(actual / "captures.npz") as saved:
            arrays = dict(saved)
        arrays["linear"][30, 40, 0] += 1
        binding = next(
            b for b in manifest["native_buffers"].values() if b["array"] == "linear"
        )
        binding["crop_sha256"] = hashlib.sha256(arrays["linear"].tobytes()).hexdigest()
        with tempfile.TemporaryDirectory(prefix="outshine-plane-native-") as temp:
            path = Path(temp)
            np.savez_compressed(path / "captures.npz", **arrays)
            manifest.update(
                npz_sha256=digest(path / "captures.npz"),
                npz_bytes=(path / "captures.npz").stat().st_size,
            )
            (path / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "full native query"):
                read_query(path, DATA / "source-512", DATA / "high-sampling")

    def test_changed_query_metadata_is_not_a_matching_source_frame(self):
        manifest = json.loads((DATA / "samples-512/manifest.json").read_text())
        manifest["query"]["eye"][0] += 1
        with tempfile.TemporaryDirectory(prefix="outshine-plane-query-") as temp:
            path = Path(temp)
            (path / "captures.npz").symlink_to(DATA / "samples-512/captures.npz")
            (path / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "query/source/original"):
                read_query(path, DATA / "source-512", DATA / "high-sampling")

    def test_component_images_preserve_cancellation_instead_of_adding_norms(self):
        grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 20, 8, 8)
        native = np.zeros((8, 8, 4))
        native[3, 3, 3] = 0.5
        actual = np.zeros_like(native)
        actual[3, 3, 3] = 0.55
        candidate = np.zeros_like(native)
        candidate[3, 3, 3] = 0.8
        geometry = {
            name: np.zeros((8, 8))
            for name in (
                "original_geos_alpha",
                "flat_continuous_capture_alpha",
                "fitted_continuous_capture_alpha",
            )
        }
        geometry["original_geos_alpha"][3, 3] = 0.6
        geometry["flat_continuous_capture_alpha"][3, 3] = 0.4
        geometry["fitted_continuous_capture_alpha"][3, 3] = 0.5
        ideal = np.zeros((4, 4), bool)
        ideal[1, 1] = True
        report, maps = signed_components(
            native, actual, candidate, ideal, geometry, grid, view
        )
        np.testing.assert_allclose(
            sum(v for k, v in maps.items() if k != "native_combined"),
            actual[:, :, 3] - native[:, :, 3],
            atol=1e-14,
        )
        self.assertGreater(
            sum(report["component_l1_over_native_original"].values()),
            report["native_combined_l1"],
        )
        self.assertAlmostEqual(report["native_combined_l1"], 0.1)

    def test_geometry_outside_capture_is_declined_before_projecting_a_subset(self):
        source = {
            "positions": np.array([[2, 0, 0], [0, 1, 0], [0, 0, 0]], np.float32),
            "indices": np.array([[0, 1, 2]]),
        }
        grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 20, 8, 8)
        with self.assertRaisesRegex(ValueError, "Whole original"):
            geometry_controls(source, grid, view, FieldPlane())

    def test_actual_opponents_and_independent_areas_improve_without_source_changes(
        self,
    ):
        source = read_source(DATA / "source-512", DATA / "high-sampling")
        original, _ = read_original(DATA / "original-geometry")
        saved = copy.deepcopy(source[1]["source_depth"])
        report, images = analyse(
            source,
            readbacks(DATA),
            readbacks(DATA / "high-sampling"),
            readbacks(DATA / "same-rays"),
            original,
            read_query(
                DATA / "samples-512", DATA / "source-512", DATA / "high-sampling"
            ),
        )
        self.assertEqual(report["fit"]["source_points"], 43049)
        self.assertEqual(report["unique_poses"], 4)
        self.assertEqual(len(report["rows"]), 5)
        self.assertLess(
            report["coverage_components"]["signed_identity_maximum_error"], 1e-12
        )
        for row in report["rows"]:
            old, new = [
                row["variants"][name]["original_vs_candidate"]
                for name in ("flat", "fitted")
            ]
            self.assertLess(new["rgb_relative_l1"], old["rgb_relative_l1"])
            self.assertLess(new["alpha_relative_l1"], old["alpha_relative_l1"])
        for control in report["projected_cell_controls"].values():
            self.assertLess(control["independent_union_alpha_max_error"], 1e-10)
        self.assertLess(
            report["projected_cell_controls"]["flat"]["projected_cell_field_vs_prefix"][
                "rgb_relative_l1"
            ],
            1e-10,
        )
        self.assertLess(
            report["projected_cell_controls"]["fitted"][
                "original_vs_projected_cell_field"
            ]["rgb_relative_l1"],
            0.045,
        )
        np.testing.assert_array_equal(saved, source[1]["source_depth"])
        self.assertEqual(images["stage_native_combined"].shape, (96, 128))


if __name__ == "__main__":
    unittest.main()
