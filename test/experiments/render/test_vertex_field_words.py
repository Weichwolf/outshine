"""Native archive word oracles and bounded token protocol counterexamples."""

import hashlib
import struct
import unittest

import numpy as np
from vertex_field_cases import cases, shader_records
from vertex_field_oracle import scalar_decode
from vertex_field_words import HEADER, MAX_VERTICES, decode, encode


def source(n=3):
    positions = np.zeros((n, 3), "<f4")
    positions[:, 0] = np.arange(n)
    return {
        "positions": positions,
        "uv": np.zeros((n, 2), "<f4"),
        "normal": np.full(n, 0x80008000, "<u4"),
        "metres": np.zeros((n, 2), "<f4"),
        "colour": np.ones((n, 4), "<f4"),
        "indices": np.array([[0, 1, 2]], "<u4") if n else np.empty((0, 3), "<u4"),
    }


class VertexFieldWords(unittest.TestCase):
    def test_all_native_case_fields_match_scalar_and_vector_decoders_and_archive_corners(
        self,
    ):
        total = 0
        for name, fields, raw, proof in cases():
            expected = shader_records(fields)

            self.assertEqual(
                hashlib.sha256(expected).hexdigest(), proof["expanded_native_sha256"]
            )
            original = {
                k: v.tobytes()
                for k, v in fields.items()
                if k
                in (
                    "positions",
                    "uv",
                    "normal",
                    "metres",
                    "colour",
                    "indices",
                    "binding",
                )
            }
            for mode in ("all", "position-normal"):
                with self.subTest(case=name, mode=mode):
                    candidate = encode(fields, mode=mode, raw_core_bytes=len(raw))
                    self.assertIsNone(candidate.reason)
                    self.assertLess(len(candidate.payload), len(raw))
                    for decoded in (
                        decode(candidate.payload),
                        scalar_decode(candidate.payload),
                    ):
                        self.assertEqual(shader_records(decoded), expected)
                        for key in ("positions", "uv", "normal", "metres", "colour"):
                            self.assertEqual(
                                decoded[key].tobytes(), fields[key].tobytes()
                            )
                        np.testing.assert_array_equal(
                            decoded["indices"], fields["indices"]
                        )
            for key, data in original.items():
                self.assertEqual(fields[key].tobytes(), data)
            total += 1
        self.assertEqual(total, 6)

    def test_signed_zero_subnormals_and_raw_hdr_alpha_preserve_bits(self):
        fields = source()
        fields["uv"].view("<u4")[:] = [
            [0, 0x80000000],
            [1, 0x80000001],
            [0x7F7FFFFF, 2],
        ]
        fields["colour"][:] = [
            [1.53846, -0.25, 2.0, 0.125],
            [0, 0, 0, -0.0],
            [1, 1, 1, 1],
        ]
        fields["metres"].view("<u4")[2, 0] = 0x80000000
        fields["normal"][:] = [0xFFFFFFFF, 0, 1]
        for mode in ("all", "position-normal"):
            encoded = encode(fields, mode=mode)
            for out in (decode(encoded.payload), scalar_decode(encoded.payload)):
                self.assertEqual(shader_records(out), shader_records(fields))

    def test_fields_are_independent_and_duplicates_do_not_weld_vertex_identity(self):
        fields = source()
        fields["positions"][:] = [1, 2, 3]
        fields["uv"][:, 0] = [0, 256, 512]
        encoded = encode(fields)
        self.assertEqual(encoded.report["tables"]["positions"]["records"], 1)
        self.assertEqual(encoded.report["tables"]["uv"]["records"], 3)
        out = decode(encoded.payload)
        self.assertEqual(len(out["positions"]), 3)
        self.assertEqual(shader_records(out), shader_records(fields))

    def test_exact_u16_limit_and_excess_return_existing_raw_without_truncation(self):
        fitting = source(65536)
        fitting["indices"][0] = [0, 1, 65535]
        encoded = encode(fitting)
        self.assertIsNone(encoded.reason)
        self.assertEqual(encoded.report["tables"]["positions"]["records"], 65536)
        self.assertEqual(
            shader_records(decode(encoded.payload)), shader_records(fitting)
        )
        oversized = source(65537)
        original = oversized["positions"].tobytes()
        decline = encode(oversized)
        self.assertIsNone(decline.payload)
        self.assertIn("u16", decline.reason)
        self.assertEqual(original, oversized["positions"].tobytes())

    def test_size_rejection_and_empty_body_keep_raw(self):
        fields = source()
        self.assertIsNone(encode(fields, raw_core_bytes=1).payload)
        self.assertIsNone(encode(source(0)).payload)
        fields["indices"] = np.empty((0, 3), "<u4")
        self.assertIsNone(encode(fields).payload)

    def test_truncation_trailing_and_invalid_counts_reject_before_arrays(self):
        payload = encode(source()).payload
        for data in (b"", payload[:47], payload[:-1], payload + b"\0"):
            with self.assertRaises(ValueError):
                decode(data)
        for field, value in ((1, MAX_VERTICES + 1), (3, 8), (4, 7), (5, 4), (7, 65537)):
            header = list(HEADER.unpack_from(payload))
            header[field] = value
            damaged = HEADER.pack(*header) + payload[HEADER.size :]
            with self.assertRaises(ValueError):
                decode(damaged)
        with self.assertRaises(TypeError):
            decode(bytearray(payload))

    def test_corrupt_dictionary_and_geometry_indices_reject(self):
        payload = encode(source()).payload
        bad = bytearray(payload)
        struct.pack_into("<I", bad, HEADER.size, 65535)
        with self.assertRaises(ValueError):
            decode(bytes(bad))
        bad = bytearray(payload)
        struct.pack_into("<H", bad, len(bad) - 2, 3)
        with self.assertRaises(ValueError):
            decode(bytes(bad))

    def test_invalid_native_shapes_values_and_budget_types_reject(self):
        for key, value in (
            ("uv", np.zeros((3, 3), "<f4")),
            ("normal", np.zeros(3, "<f4")),
            ("positions", np.full((3, 3), np.inf, "<f4")),
            ("indices", np.array([[0.0, 1.0, 2.0]])),
            ("indices", np.array([[0, 1, 3]])),
        ):
            with self.assertRaises(ValueError):
                encode(source() | {key: value})
        for raw in (-1, True, 2.5):
            with self.assertRaises(ValueError):
                encode(source(), raw_core_bytes=raw)


if __name__ == "__main__":
    unittest.main()
