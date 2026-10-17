"""Admission boundary, primitive-array safety and exact restored surface behavior."""

import hashlib
import io
import json
import unittest
import warnings
import zipfile
from unittest.mock import patch

import numpy as np
import test_surface_patches
from outshine_ref.contracts import LocalFrame
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from stair_prepared_surface_archive import pack_surface
from stair_prepared_surface_restore import restore_surface
from stair_resident_snapshot_verify import equal_patches


def sha(payload):
    return hashlib.sha256(payload).hexdigest()


def change(payload, *, arrays=None, meta=None):
    with np.load(io.BytesIO(payload), allow_pickle=False) as archive:
        values = {k: archive[k] for k in archive.files}
    if meta is not None:
        description = json.loads(values["metadata"].tobytes())
        meta(description)
        values["metadata"] = np.frombuffer(
            json.dumps(description).encode(), dtype=np.uint8
        )
    if arrays is not None:
        arrays(values)
    output = io.BytesIO()
    np.savez(output, **values)
    return output.getvalue()


class PreparedSurface(unittest.TestCase):
    def setUp(self):
        root = test_surface_patches.patch("root1")
        self.view, _ = SurfacePatches(root).replace(
            test_surface_patches.refined("next2"), {"root1": [0, 1]}
        )
        self.frame = root.frame
        self.payload = pack_surface(self.view)
        # The external pin is taken from a normally constructed known-valid product.
        self.pin = sha(self.payload)

    def restore(self, payload=None, **kwargs):
        return restore_surface(
            self.payload if payload is None else payload,
            expected_sha256=kwargs.pop("expected_sha256", self.pin),
            frame=kwargs.pop("frame", self.frame),
            **kwargs,
        )

    def test_stored_and_compressed_roundtrips_keep_all_data_and_queries(self):
        for compressed in (False, True):
            payload = pack_surface(self.view, compressed=compressed)
            actual = self.restore(payload, expected_sha256=sha(payload))
            equal_patches(actual, self.view)
            self.assertEqual(actual.max_patches, self.view.max_patches)
            before, after = (
                self.view.query([-1, -1, 3, 3]),
                actual.query([-1, -1, 3, 3]),
            )
            self.assertEqual(before.face_keys, after.face_keys)
            np.testing.assert_array_equal(before.mesh.vertices, after.mesh.vertices)
            np.testing.assert_array_equal(before.mesh.triangles, after.mesh.triangles)
            with self.assertRaises(ValueError):
                actual.active_faces["next2"].setflags(write=True)
            with self.assertRaises(ValueError):
                actual.patches[1].face_data["source"].setflags(write=True)

    def test_admitted_restore_does_not_repeat_geometric_validation_or_history(self):
        with (
            patch.object(
                SurfacePatch,
                "__post_init__",
                side_effect=AssertionError("geometry repeated"),
            ),
            patch.object(
                SurfacePatches,
                "replace",
                side_effect=AssertionError("history repeated"),
            ),
        ):
            actual = self.restore()
        equal_patches(actual, self.view)

    def test_new_generation_still_uses_normal_strict_replacement(self):
        actual = self.restore()
        after, _ = actual.replace(
            test_surface_patches.refined("last3"), {"next2": [0, 1, 2, 3]}
        )
        self.assertFalse(after.active_faces["next2"].any())
        self.assertTrue(after.active_faces["last3"].all())
        self.assertTrue(actual.active_faces["next2"].all())

    def test_missing_wrong_pin_mutable_bytes_frame_and_budget_are_refused(self):
        for kwargs in (
            {"expected_sha256": None},
            {"expected_sha256": "0" * 64},
            {"frame": LocalFrame("different-frame")},
            {"max_bytes": len(self.payload) - 1},
            {"max_bytes": False},
        ):
            with self.assertRaises((ValueError, TypeError)):
                self.restore(**kwargs)
        with self.assertRaises(ValueError):
            self.restore(bytearray(self.payload))

    def test_changed_geometry_and_masks_cannot_rebind_the_external_pin(self):
        for key in ("p1-v", "p1-active", "p1-a0"):

            def edit(arrays, field=key):
                arrays[field].flat[0] = 1 - arrays[field].flat[0]

            changed = change(self.payload, arrays=edit)
            self.assertNotEqual(sha(changed), self.pin)
            with self.assertRaises(ValueError):
                self.restore(changed)

    def test_format_extra_valid_flag_and_duplicate_patch_ids_are_refused(self):
        for edit in (
            lambda m: m.update(format="unknown/2"),
            lambda m: m.update(valid=True),
            lambda m: m["patches"][1].update(id=m["patches"][0]["id"]),
        ):
            changed = change(self.payload, meta=edit)
            with self.assertRaises(ValueError):
                self.restore(changed, expected_sha256=sha(changed))

    def test_structural_guards_remain_even_for_a_wrongly_pinned_archive(self):
        for key, value in (
            ("p1-f", np.full((4, 3), 999999, dtype=np.uint32)),
            ("p1-v", np.full((5, 3), np.nan)),
            ("p1-active", np.ones(3, dtype=bool)),
            ("p1-a0", np.zeros(4, dtype=object)),
        ):
            changed = change(
                self.payload, arrays=lambda a, k=key, v=value: a.update({k: v})
            )
            with self.assertRaises(ValueError):
                self.restore(changed, expected_sha256=sha(changed))

    def test_false_npy_size_and_duplicate_zip_member_fail_before_array_load(self):
        output = io.BytesIO()
        with (
            zipfile.ZipFile(io.BytesIO(self.payload)) as old,
            zipfile.ZipFile(output, "w") as new,
        ):
            for member in old.infolist():
                data = old.read(member)
                if member.filename == "p1-v.npy":
                    data = data.replace(b"(5, 3)", b"(9, 3)", 1)
                new.writestr(member.filename, data)
        changed = output.getvalue()
        with (
            patch(
                "stair_prepared_surface_restore.np.load",
                side_effect=AssertionError("unbounded allocation"),
            ),
            self.assertRaises(ValueError),
        ):
            self.restore(changed, expected_sha256=sha(changed))
        output = io.BytesIO(self.payload)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(output, "a") as archive:
                archive.writestr("metadata.npy", b"duplicate")
        changed = output.getvalue()
        with self.assertRaises(ValueError):
            self.restore(changed, expected_sha256=sha(changed))


if __name__ == "__main__":
    unittest.main()
