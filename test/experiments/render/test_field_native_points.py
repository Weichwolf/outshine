"""Native capture/observer identities and valid warm bytes stay separate from proof."""

import copy
import hashlib
import tempfile
import unittest
from pathlib import Path

import numpy as np
from field_external_depth import SampleBinding, compose_registered
from field_native_points import native_binding, read_native_points


def record():
    return {
        "query": "same-name-is-not-identity",
        "field_asset_key": "whole-native-view-product",
        "source_readback_bytes": 80,
        "source_depth": {
            "format": "reverse_z_0_1_float32",
            "width_px": 2,
            "height_px": 2,
            "bytes": 16,
            "eye_m": [0, 0, 10],
            "right": [1, 0, 0],
            "up": [0, 1, 0],
            "forward": [0, 0, -1],
            "orthographic": True,
            "x_mag_m": 2.0,
            "y_mag_m": 2.0,
            "yfov_rad": 0.0,
            "near_m": 1.0,
            "far_m": 20.0,
        },
        "lighting_observer": {
            "eye_m": [1, 2, 15],
            "forward": [0, 0, -1],
            "orthographic": False,
        },
    }


def provenance():
    return SampleBinding(
        "verified-source-frame",
        "requested-rays",
        "world-r1",
        ("whole-native-view-product",),
        "materials-r1",
        "actual-light-r1",
        "requested-observer",
    )


class NativePointTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-native-points-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.colour, self.depth = (
            self.root / "source-field.f32",
            self.root / "source-depth.f32",
        )
        self.rgba = np.array(
            [[[2, 1, 0, 1], [0, 0, 0, 0]], [[1, 3, 2, 1], [0, 0, 0, 0]]], dtype="<f4"
        )
        self.z = np.array([[0.8, 0], [0.3, 0]], dtype="<f4")
        self.rgba.tofile(self.colour)
        self.z.tofile(self.depth)
        self.pins = {
            "colour_sha256": hashlib.sha256(self.colour.read_bytes()).hexdigest(),
            "depth_sha256": hashlib.sha256(self.depth.read_bytes()).hexdigest(),
        }

    def read(self, metadata=None):
        return read_native_points(
            self.colour, self.depth, metadata or record(), provenance(), **self.pins
        )

    def test_actual_values_borrow_immutable_bytes_but_do_not_grant_visibility(self):
        result = self.read()
        np.testing.assert_array_equal(result.rgba, self.rgba)
        np.testing.assert_array_equal(result.reverse_z, self.z)
        self.assertFalse(
            result.rgba.flags.writeable or result.reverse_z.flags.writeable
        )
        self.assertFalse(result.known.any())
        decided = compose_registered(
            result, result, wanted_a=result.binding, wanted_b=result.binding
        )
        self.assertEqual(decided.reason, "incomplete_sample_domain")
        self.assertIsNone(decided.rgba)
        self.assertEqual(result.binding.frame, provenance().frame)
        self.assertEqual(result.binding.lighting, provenance().lighting)

    def test_warm_zero_readback_telemetry_preserves_the_complete_held_payload(self):
        warm = record()
        warm["source_readback_bytes"] = 0
        actual = self.read(warm)
        np.testing.assert_array_equal(actual.rgba, self.rgba)
        self.assertEqual(actual.binding, self.read().binding)

    def test_new_payload_with_same_source_and_camera_changes_sample_revision(self):
        before = self.read()
        changed = self.rgba.copy()
        changed[0, 0, 0] += 0.01
        changed.tofile(self.colour)
        self.pins["colour_sha256"] = hashlib.sha256(
            self.colour.read_bytes()
        ).hexdigest()
        after = self.read()
        self.assertEqual(before.binding.sources, after.binding.sources)
        self.assertEqual(before.binding.capture, after.binding.capture)
        self.assertNotEqual(
            before.binding.sample_revision, after.binding.sample_revision
        )
        result = compose_registered(
            after, before, wanted_a=before.binding, wanted_b=before.binding
        )
        self.assertEqual(result.reason, "changed_source_material_or_demand")

    def test_pbr_identity_uses_eye_for_perspective_direction_for_orthographic(self):
        original = record()
        base, _ = native_binding(original, provenance())
        rotated = copy.deepcopy(original)
        rotated["lighting_observer"]["forward"] = [1, 0, 0]
        self.assertEqual(native_binding(rotated, provenance())[0], base)
        rotated["lighting_observer"]["eye_m"] = [2, 2, 15]
        self.assertNotEqual(
            native_binding(rotated, provenance())[0].pbr_view, base.pbr_view
        )
        original["lighting_observer"]["orthographic"] = True
        base, _ = native_binding(original, provenance())
        original["lighting_observer"]["eye_m"] = [3, 8, 12]
        self.assertEqual(native_binding(original, provenance())[0], base)
        original["lighting_observer"]["forward"] = [1, 0, 0]
        self.assertNotEqual(
            native_binding(original, provenance())[0].pbr_view, base.pbr_view
        )

    def test_capture_changes_and_same_query_name_do_not_alias(self):
        changed = record()
        changed["source_depth"]["eye_m"][0] = 0.001
        self.assertNotEqual(
            self.read(changed).binding.capture, self.read().binding.capture
        )
        changed = record()
        changed["query"] = "different-query-name"
        self.assertEqual(self.read(changed).binding, self.read().binding)

    def test_native_double_value_identity_does_not_depend_on_json_number_spelling(self):
        metadata = record()
        metadata["source_depth"]["near_m"] = 1
        self.assertEqual(
            self.read(metadata).binding.capture, self.read().binding.capture
        )
        metadata["source_depth"]["near_m"] = np.nextafter(1.0, 2.0).item()
        self.assertNotEqual(
            self.read(metadata).binding.capture, self.read().binding.capture
        )

    def test_unknown_stale_or_over_capacity_headers_fail_before_opening_files(self):
        bad = []
        value = record()
        value.pop("lighting_observer")
        bad.append(value)
        value = record()
        value["field_asset_key"] = "different-view"
        bad.append(value)
        value = record()
        value["source_depth"]["bytes"] = 0
        bad.append(value)
        value = record()
        value["source_depth"]["width_px"] = 1000000000
        bad.append(value)
        value = record()
        value["lighting_observer"]["forward"] = [0, 0, -2]
        bad.append(value)
        value = record()
        value["source_depth"]["right"] = [2, 0, 0]
        bad.append(value)
        self.colour.unlink()
        self.depth.unlink()
        for metadata in bad:
            with self.subTest(metadata=metadata), self.assertRaises(ValueError):
                self.read(metadata)

    def test_changed_truncated_or_uncoupled_payloads_reject(self):
        raw = self.colour.read_bytes()
        for changed in (raw[:-4], bytes([raw[0] ^ 1]) + raw[1:]):
            self.colour.write_bytes(changed)
            with self.assertRaises(ValueError):
                self.read()
        self.colour.write_bytes(raw)
        bad = self.z.copy()
        bad[0, 1] = 0.5
        bad.tofile(self.depth)
        self.pins["depth_sha256"] = hashlib.sha256(self.depth.read_bytes()).hexdigest()
        with self.assertRaises(ValueError):
            self.read()


if __name__ == "__main__":
    unittest.main()
