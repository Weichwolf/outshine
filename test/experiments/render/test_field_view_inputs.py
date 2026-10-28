"""Existing native wire binding, corruption and coupled-sample constraints."""

import struct
import unittest

import numpy as np
from field_view_inputs import MATERIAL, TEXEL, decode_view, fnv


def native_blob(*, depth=0.625, normal=(0, 0, 1), surface=1):
    texels = np.zeros((3, 3), TEXEL)
    texels[1, 1] = (normal, depth, surface)
    data = (
        struct.pack(
            "<QIQIII4d", 0x004E574F5243534F, 2, fnv(b"bound-view"), 3, 1, 1, 0, 0, 0, 4
        )
        + bytes(52)
        + struct.pack("<3d", 0, 0, 1)
        + texels.tobytes()
        + np.zeros((3, 3), MATERIAL).tobytes()
    )
    return data + struct.pack("<Q", fnv(data))


class InputTests(unittest.TestCase):
    def test_fnv_published_vectors_and_native_owned_payload(self):
        self.assertEqual(fnv(b""), 0xCBF29CE484222325)
        self.assertEqual(fnv(b"hello"), 0xA430D84680AABD0B)
        atlas = decode_view(native_blob(), "bound-view")
        self.assertEqual(atlas["pixels"], 3)
        self.assertEqual(atlas["texels"]["depth"][1, 1], 0.625)
        with self.assertRaises(ValueError):
            atlas["texels"]["depth"][1, 1] = 0

    def test_corrupt_payload_binding_and_truncation_are_rejected(self):
        raw = native_blob()
        changed = raw[:200] + bytes([raw[200] ^ 1]) + raw[201:]
        for data, key in (
            (raw, "different-view"),
            (raw[:-1], "bound-view"),
            (changed, "bound-view"),
            (b"", "bound-view"),
        ):
            with self.subTest(key=key, size=len(data)), self.assertRaises(ValueError):
                decode_view(data, key)

    def test_consistent_checksum_does_not_allow_invalid_coupled_samples(self):
        for options in (
            {"depth": 0},
            {"depth": 1.1},
            {"normal": (0, 0, 0)},
            {"surface": 2},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                decode_view(native_blob(**options), "bound-view")


if __name__ == "__main__":
    unittest.main()
