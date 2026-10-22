"""Whole-core native byte equality and cross-word/lossless-range counterexamples."""

import struct
import unittest

import numpy as np
from packed_position_oracle import scalar_decode
from packed_position_words import HEADER, MAGIC, decode, encode, position_words
from vertex_field_cases import cases, shader_records


def core(words):
    words = np.tile(np.asarray(words, "<u4"), (32, 1))
    raw = b"prefix" + words.tobytes() + b"native attributes and topology"
    return raw, [[6, len(words), 12]]


class PackedPositions(unittest.TestCase):
    def test_all_original_cores_and_shader_positions_are_word_identical(self):
        used = []
        for name, source, raw, receipt in cases():
            with self.subTest(case=name):
                spans = receipt["vertex_spans"]
                self.assertEqual(
                    position_words(raw, spans).tobytes(), source["positions"].tobytes()
                )
                candidate = encode(raw, spans)
                if name.startswith("near-cell"):
                    self.assertIsNone(candidate.payload)
                    self.assertEqual(candidate.report["sum_bits"], 72)
                    continue
                self.assertIsNone(candidate.reason)
                self.assertLess(len(candidate.payload), len(raw))
                for restored in (
                    decode(candidate.payload, spans),
                    scalar_decode(candidate.payload, spans),
                ):
                    self.assertEqual(restored, raw)
                    current = source | {
                        "positions": position_words(restored, spans).view("<f4")
                    }
                    self.assertEqual(shader_records(current), shader_records(source))
                used.append(name)
        self.assertEqual(len(used), 4)

    def test_every_field_width_and_crossing_decodes_with_uint32_only(self):
        for widths in (
            [0, 0, 0],
            [32, 0, 0],
            [0, 32, 32],
            [31, 32, 1],
            [1, 32, 31],
            [21, 24, 19],
        ):
            with self.subTest(widths=widths):
                bases = [0, 0, 0]
                integers = [(1 << width) - 1 for width in widths]
                # Build known packets directly, so all widths are tested without
                # imposing the finite-float constraints on artificial bit ranges.
                # Largest finite negative word also exercises full32 extraction.
                values = [
                    min(value, 0xFF7FFFFF if width == 32 else 0x7F7FFFFF)
                    for value, width in zip(integers, widths, strict=True)
                ]
                packed = sum(v << sum(widths[:i]) for i, v in enumerate(values))
                n = 32
                payload = HEADER.pack(MAGIC, n, n * 12, *bases, *widths)
                payload += struct.pack("<Q", packed) * n
                expected = struct.pack("<3I", *values) * n
                spans = [[0, n, 12]]
                self.assertEqual(decode(payload, spans), expected)
                self.assertEqual(scalar_decode(payload, spans), expected)

    def test_signed_zero_subnormal_sign_change_and_negative_floats(self):
        for values in (
            [[0, 0x80000000, 1], [0x80000000, 0x80000000, 0x80000001]],
            [
                [0xBF800000, 0x40000000, 0xC0000000],
                [0xBF800001, 0x40000001, 0xC0000001],
            ],
        ):
            raw, spans = core(values)
            candidate = encode(raw, spans)
            self.assertIsNotNone(candidate.payload)
            self.assertEqual(decode(candidate.payload, spans), raw)
            self.assertEqual(scalar_decode(candidate.payload, spans), raw)

    def test_overwide_and_small_or_empty_cores_keep_raw(self):
        raw, spans = core([[0x3F800000] * 3, [0xBF800000] * 3])
        candidate = encode(raw, spans)
        self.assertIsNone(candidate.payload)
        self.assertEqual(candidate.report["sum_bits"], 96)
        self.assertIsNone(encode(b"", [[0, 0, 12]]).payload)
        self.assertIsNone(encode(b"\0" * 12, [[0, 1, 12]]).payload)

    def test_nonfinite_positions_and_invalid_layout_reject(self):
        raw, spans = core([[0x7FC00001, 0, 0]])
        with self.assertRaises(ValueError):
            encode(raw, spans)
        raw, spans = core([[0, 0, 0]])
        for bad in (
            [],
            [[6, -1, 12]],
            [[6, 999999, 12]],
            [[6, 32, 16]],
            [[6, True, 12]],
            [[6, 32, 12], [6, 0, 12]],
        ):
            with self.subTest(layout=bad), self.assertRaises(ValueError):
                encode(raw, bad)

    def test_bad_header_length_padding_and_overflow_reject(self):
        raw, spans = core([[0x3F800000, 0x40000000, 0x40400000]])
        payload = encode(raw, spans).payload
        for broken in (b"", payload[:-1], payload + b"x", b"badmagic" + payload[8:]):
            with self.subTest(length=len(broken)), self.assertRaises(ValueError):
                decode(broken, spans)
        data = bytearray(payload)
        data[HEADER.size + 6] = 1
        with self.assertRaises(ValueError):
            decode(bytes(data), spans)
        for field, value in ((1, 0), (6, 33), (6, 32), (7, 32)):
            header = list(HEADER.unpack_from(payload))
            header[field] = value
            if field == 7:
                header[6] = 32
                header[8] = 1
            data = HEADER.pack(*header) + payload[HEADER.size :]
            if field == 6 and value == 32:
                # A maximal delta with a positive base must not wrap uint32.
                data = data[: HEADER.size + 6] + b"\xff" * 4 + data[HEADER.size + 10 :]
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                decode(data, spans)


if __name__ == "__main__":
    unittest.main()
