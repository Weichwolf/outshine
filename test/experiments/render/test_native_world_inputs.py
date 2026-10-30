"""Actual whole native inputs, independent union and preallocation refusals."""

import copy
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from zipfile import ZIP_DEFLATED, ZipFile

import numpy as np
from building_sample_inputs import full_buffer_digest
from compare_native_world_fields import capture_for_pose, composition, reduce_samples
from field_depth_reprojection import Reprojected
from native_world_inputs import DATA, bound_archive, digest, load


class NativeWorldTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, cls.arrays, cls.cases, cls.original = load()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_research.world."
        )
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("manifest.json", "samples.npz"):
            shutil.copyfile(DATA / name, self.root / name)
        self.changed = copy.deepcopy(self.manifest)

    def save(self):
        (self.root / "manifest.json").write_text(
            json.dumps(self.changed, indent=2) + "\n"
        )

    def test_actual_whole_fixture_is_lossless_and_does_not_invent_runtime_values(self):
        self.assertEqual(len(self.arrays), 36)
        self.assertEqual(len({id(v) for v in self.arrays.values()}), 23)
        self.assertEqual(len(self.original["indices"]), 38476)
        self.assertIsNone(self.manifest["runtime_world_from_field"])
        self.assertIsNone(self.manifest["runtime_world_revision"])
        self.assertTrue(all(not a.flags.writeable for a in self.arrays.values()))

    def test_changed_executed_camera_is_refused_even_after_rehash(self):
        execution = self.changed["executions"]["capture"]
        execution["input"]["queries"][0]["capture"]["eye_m"][0] += 1
        execution["input_sha256"] = digest(
            (json.dumps(execution["input"], indent=2) + "\n").encode()
        )
        self.save()
        with self.assertRaisesRegex(ValueError, "effective capture"):
            load(self.root)

    def test_changed_crop_cannot_reinterpret_actual_native_points(self):
        crop = self.changed["crops_xyxy"]["near-direction"]
        crop[0] += 1
        crop[2] += 1
        self.save()
        with self.assertRaisesRegex(ValueError, "Full native buffer differs"):
            load(self.root)

    def test_union_is_independently_checked_after_all_hashes_are_amended(self):
        logical = "union-terrain/near-direction/rgba"
        key = self.changed["arrays"][logical]["array"]
        with np.load(self.root / "samples.npz", allow_pickle=False) as saved:
            physical = {name: saved[name] for name in saved.files}
        value = physical[key]
        y, x = np.argwhere(value[:, :, 3] > 0)[0]
        value[y, x, 0] += 0.125
        np.savez_compressed(self.root / "samples.npz", **physical)
        self.changed["npz_sha256"] = digest((self.root / "samples.npz").read_bytes())
        for name, record in self.changed["arrays"].items():
            if record["array"] == key:
                record["sha256"] = digest(value.tobytes())
                layer, pose, _channel = name.split("/")
                self.changed["executions"][layer]["buffers"][name] = full_buffer_digest(
                    value,
                    [0, 0, 0, 0],
                    self.changed["raster"],
                    self.changed["crops_xyxy"][pose],
                )
        self.save()
        with self.assertRaisesRegex(ValueError, "original point union differs"):
            load(self.root)

    def test_sparse_oversized_archive_is_refused_before_payload_read_or_numpy(self):
        with (self.root / "samples.npz").open("r+b") as stream:
            stream.truncate(1024**3)
        with (
            patch(
                "native_world_inputs.Path.read_bytes",
                side_effect=AssertionError("oversized payload read"),
            ),
            patch(
                "native_world_inputs.np.load",
                side_effect=AssertionError("oversized NumPy allocation"),
            ),
            self.assertRaisesRegex(ValueError, "Bounded unchanged"),
        ):
            load(self.root)

    def test_forged_large_npy_shape_is_refused_before_numpy_allocation(self):
        path = self.root / "samples.npz"
        with ZipFile(path) as archive:
            members = {name: archive.read(name) for name in archive.namelist()}
        target = (
            self.manifest["arrays"]["capture/near-direction/rgba"]["array"] + ".npy"
        )
        header = io.BytesIO()
        np.lib.format.write_array_header_1_0(
            header, {"shape": (2**34, 4), "fortran_order": False, "descr": "<f4"}
        )
        members[target] = header.getvalue() + bytes(4)
        # Keep this malformed-header control below the separate compressed-size gate.
        with ZipFile(path, "w", compression=ZIP_DEFLATED) as archive:
            for name, value in members.items():
                archive.writestr(name, value)
        self.changed["npz_sha256"] = digest(path.read_bytes())
        self.save()
        with (
            patch(
                "native_world_inputs.np.load",
                side_effect=AssertionError("forged NumPy allocation"),
            ),
            self.assertRaisesRegex(ValueError, "primitive native row length"),
        ):
            load(self.root)

    def test_pbr_reuse_uses_actual_observer_not_query_name(self):
        rgba, _depth = capture_for_pose(
            self.arrays, self.cases, "near-direction", "repeat"
        )
        np.testing.assert_array_equal(rgba, self.arrays["capture/near-direction/rgba"])
        with self.assertRaisesRegex(ValueError, "PBR observer"):
            capture_for_pose(
                self.arrays, self.cases, "revealing-side", "near-direction"
            )

    def test_decode_consumes_the_same_pinned_bytes_as_header_and_hash_checks(self):
        def invalidate_disk_after_check(payload, manifest):
            bound_archive(payload, manifest)
            (self.root / "samples.npz").write_bytes(b"replaced after validation")

        with patch(
            "native_world_inputs.bound_archive", side_effect=invalidate_disk_after_check
        ):
            _manifest, arrays, _cases, _original = load(self.root)
        for key in arrays:
            np.testing.assert_array_equal(arrays[key], self.arrays[key])

    def test_sample_reduction_keeps_global_phase_at_an_odd_crop_origin(self):
        crop = [3, 1, 6, 5]
        tile = np.arange(12, dtype=np.float32).reshape(4, 3, 1)
        full = np.zeros((8, 8, 1), np.float32)
        full[1:5, 3:6] = tile
        expected = np.zeros((4, 4, 1))
        for y in range(4):
            for x in range(4):
                expected[y, x] = (
                    sum(
                        float(full[2 * y + dy, 2 * x + dx, 0])
                        for dy in range(2)
                        for dx in range(2)
                    )
                    / 4
                )
        np.testing.assert_array_equal(reduce_samples(tile, crop, 2), expected[:3, 1:3])

    def test_opaque_union_cannot_hide_visible_source_error_or_fake_fallback(self):
        source, other = np.zeros((3, 3, 4), np.float32), np.zeros((3, 3, 4), np.float32)
        source[1, 1] = [1, 0, 0, 1]
        other[:] = [0, 1, 0, 1]
        source_z = source[:, :, 3] * 0.8
        outer_z = np.full((3, 3), 0.5, np.float32)
        truth = other.copy()
        truth[1, 1] = source[1, 1]
        arrays = {
            "original/near-direction/rgba": source,
            "original/near-direction/depth": source_z,
        }
        for kind in ("terrain", "external"):
            arrays[kind + "/near-direction/rgba"] = other
            arrays[kind + "/near-direction/depth"] = outer_z
            arrays["union-" + kind + "/near-direction/rgba"] = truth
        model = Reprojected(
            np.zeros_like(source), np.zeros((3, 3)), np.ones((3, 3), bool), {}
        )
        result = composition(
            arrays, "near-direction", model, [0, 0, 3, 3], 1, self.root, 2.0, 0.125
        )
        for kind, value in result.items():
            self.assertEqual(value["composed_image_error"]["coverage_max"], 0)
            self.assertEqual(value["visible_source_coverage_error_max"], 1)
            self.assertTrue(value["original_group_retained"])
            self.assertEqual(value["chosen_path_error"]["rgb_max"], 0)
            with np.load(
                self.root / ("near-direction-" + kind + "-composition.npz")
            ) as saved:
                np.testing.assert_array_equal(saved["chosen"], truth)


if __name__ == "__main__":
    unittest.main()
