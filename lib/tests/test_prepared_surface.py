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
from outshine_ref.geometry.surface_patches.prepared import pack_surface, restore_surface


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


def equal_patches(actual, expected):
    assert actual.max_patches == expected.max_patches
    assert len(actual.patches) == len(expected.patches)
    for a, b in zip(actual.patches, expected.patches, strict=True):
        assert (a.patch_id, a.frame, a.kind, a.max_faces) == (
            b.patch_id,
            b.frame,
            b.kind,
            b.max_faces,
        )
        np.testing.assert_array_equal(a.mesh.vertices, b.mesh.vertices)
        np.testing.assert_array_equal(a.mesh.triangles, b.mesh.triangles)
        assert {k: v.dtype for k, v in a.face_data.items()} == {
            k: v.dtype for k, v in b.face_data.items()
        }
        for k in a.face_data:
            np.testing.assert_array_equal(a.face_data[k], b.face_data[k])
        np.testing.assert_array_equal(
            actual.active_faces[a.patch_id], expected.active_faces[b.patch_id]
        )


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
            for part in actual.patches:
                for array in (part.mesh.vertices, part.mesh.triangles):
                    with self.assertRaises(ValueError):
                        array.setflags(write=True)

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

    def test_standard_npy_byte_order_is_preserved_by_normal_mesh_conversion(self):
        def edit(arrays):
            for i in (0, 1):
                arrays[f"p{i}-v"] = arrays[f"p{i}-v"].astype(">f8")
                arrays[f"p{i}-f"] = arrays[f"p{i}-f"].astype(">u4")

        changed = change(self.payload, arrays=edit)
        actual = self.restore(changed, expected_sha256=sha(changed))
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

    def test_member_shape_and_dtype_budgets_fail_before_array_decode(self):
        for key, values in (
            ("p1-v", np.zeros((65537, 3))),
            ("p1-v", np.zeros((5, 4))),
            ("p1-v", np.zeros((5, 3), dtype=np.float32)),
            ("p1-f", np.zeros((65537, 3), dtype=np.uint32)),
            ("p1-f", np.zeros((4, 3), dtype=np.int64)),
            ("p1-active", np.zeros((4, 1), dtype=bool)),
            ("p1-active", np.zeros(4, dtype=np.uint8)),
            ("p1-a0", np.zeros((4, 1))),
            ("metadata", np.zeros(1024**2 + 1, dtype=np.uint8)),
        ):
            with self.subTest(key=key, shape=values.shape, dtype=values.dtype):
                changed = change(
                    self.payload, arrays=lambda a, k=key, v=values: a.update({k: v})
                )
                with (
                    patch(
                        "outshine_ref.geometry.surface_patches._restore.np.load",
                        side_effect=AssertionError(
                            "array decoded before size admission"
                        ),
                    ),
                    self.assertRaises(ValueError),
                ):
                    self.restore(changed, expected_sha256=sha(changed))

    def test_patch_capacity_attributes_and_canonical_mask_are_checked(self):
        for edit in (
            lambda m: m.update(max_patches=1),
            lambda m: m["patches"][1].update(max_faces=3),
            lambda m: m["patches"][1].update(attributes=["source", "source"]),
        ):
            changed = change(self.payload, meta=edit)
            with self.assertRaises(ValueError):
                self.restore(changed, expected_sha256=sha(changed))

        def noncanonical_mask(arrays):
            arrays["p1-active"].view(np.uint8)[0] = 2

        changed = change(self.payload, arrays=noncanonical_mask)
        with self.assertRaises(ValueError):
            self.restore(changed, expected_sha256=sha(changed))

    def test_restore_keeps_exact_scan_nearby_and_explicit_face_queries(self):
        from shapely.geometry import Point

        actual = self.restore()
        for method in ("index", "scan"):
            a = actual.nearby(Point(1, 1), 0.2, method=method)
            b = self.view.nearby(Point(1, 1), 0.2, method=method)
            self.assertEqual(a.face_keys, b.face_keys)
            np.testing.assert_array_equal(a.mesh.vertices, b.mesh.vertices)
            np.testing.assert_array_equal(a.mesh.triangles, b.mesh.triangles)
            a2, b2 = actual.faces(a.face_keys), self.view.faces(b.face_keys)
            np.testing.assert_array_equal(a2.mesh.vertices, b2.mesh.vertices)
            np.testing.assert_array_equal(a2.mesh.triangles, b2.mesh.triangles)

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
                "outshine_ref.geometry.surface_patches._restore.np.load",
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

    def test_incomplete_archive_and_missing_arrays_have_a_controlled_failure(self):
        for payload in (
            b"not a zip file",
            change(self.payload, arrays=lambda a: a.pop("p1-v")),
        ):
            with self.assertRaises(ValueError):
                self.restore(payload, expected_sha256=sha(payload))


if __name__ == "__main__":
    unittest.main()
