"""Native raw-feature binding and typed GroundProfile semantic countercases."""

import hashlib
import unittest

from mvt_source_profile_inputs import native_value, profile_properties


def varint(value):
    encoded = bytearray()
    while value >= 128:
        encoded.append((value & 127) | 128)
        value >>= 7
    return bytes(encoded + bytes([value]))


def blob(number, data):
    return varint((number << 3) | 2) + varint(len(data)) + data


def example(layer=b"transportation", geometry=b"\x09\x00\x00\x0a\x02\x00"):
    # indoor=number0 followed by duplicate indoor="yes"; native keeps first.
    feature = (
        b"\x08\x07"
        + blob(2, b"\x00\x00\x00\x01\x01\x02")
        + b"\x18\x02"
        + blob(4, geometry)
    )
    values = [b"\x38\x00", blob(1, b"yes"), blob(1, b"0")]
    body = (
        blob(1, layer)
        + blob(2, feature)
        + blob(3, b"indoor")
        + blob(3, b"level")
        + b"".join(blob(4, value) for value in values)
    )
    return blob(3, body), feature


class NativeFeatureProfileSemantics(unittest.TestCase):
    def test_exact_feature_wire_binding_and_first_duplicate_attribute(self):
        raw, feature = example()
        rows, work = profile_properties(raw)
        self.assertEqual(
            rows, {hashlib.sha256(feature).digest(): {"indoor": 0.0, "level": "0"}}
        )
        self.assertEqual(work["wire_features"], 1)
        self.assertEqual(work["tag_words"], 6)
        self.assertEqual(work["decoded_geometry_words"], 0)
        self.assertEqual(work["additional_geometry_payload_copy_bytes"], 0)

    def test_same_provider_id_with_changed_geometry_has_different_content(self):
        raw, old = example()
        other, new = example(geometry=b"\x09\x00\x00\x0a\x04\x00")
        self.assertNotEqual(hashlib.sha256(old).digest(), hashlib.sha256(new).digest())
        self.assertNotIn(hashlib.sha256(old).digest(), profile_properties(other)[0])
        self.assertIn(hashlib.sha256(old).digest(), profile_properties(raw)[0])

    def test_numeric_boolean_string_and_repeated_scalar_native_semantics(self):
        self.assertEqual(native_value(b"\x38\x00"), 0.0)
        self.assertEqual(native_value(b"\x38\x02"), 1.0)
        self.assertEqual(native_value(blob(1, b"0")), "0")
        self.assertEqual(native_value(b"\x28\x01\x28\x00"), 0.0)
        self.assertEqual(native_value(blob(1, b"yes") + blob(1, b"no")), "no")
        with self.assertRaises(ValueError):
            native_value(blob(1, b"0") + b"\x28\x00")

    def test_only_exact_native_streets_source_layer_is_indexed(self):
        raw, _feature = example(layer=b"building")
        rows, work = profile_properties(raw)
        self.assertEqual(rows, {})
        self.assertEqual(work["wire_features"], 0)

    def test_byte_and_tag_work_are_bounded(self):
        raw, _feature = example()
        with self.assertRaisesRegex(ValueError, "byte budget"):
            profile_properties(raw, byte_limit=len(raw) - 1)
        with self.assertRaisesRegex(ValueError, "tag budget"):
            profile_properties(raw, tag_word_limit=5)


if __name__ == "__main__":
    unittest.main()
