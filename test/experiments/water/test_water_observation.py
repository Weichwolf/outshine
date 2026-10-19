"""Historical codec controls and full original/observed geometry-to-metadata bindings."""

import copy
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

import numpy as np
from compare_water_observation import compare
from native_river_inputs import read_v2
from native_water_observation import NO_INPUT, read_v3

FIXTURES = Path(__file__).parent / "fixtures"


def encoded_control(
    identity=(1 << 64) - 1, intermittent=None, provider=b"provider", cell=None
):
    """Independent field-by-field writer, not the decoder's record dtype machinery."""
    data, offsets = bytearray(), {}

    def put(name, code, *values):
        offsets[name] = len(data)
        data.extend(struct.pack("<" + code, *values))

    def text(name, raw):
        put(name + "_size", "I", len(raw))
        data.extend(raw)

    put("version", "Q", 0x335245544157)
    put("header", "Iqqq", 1, 0, 0, 0)
    put("surfaces", "Q", 1)
    put("surface", "IIf", 0, 1, 10)
    put("surface_input", "I", 0)
    put("rings", "QII", 1, 0, 3)
    put("courses", "QIIIf", 1, 0, 2, 0, 1)
    put("course_input", "I", 0)
    put("levels", "Iff", 2, 10, 11)
    put("points", "I6d", 6, 48, 10, 48, 10.1, 48.1, 10)
    put("tiles", "QiiiII", 1, 1, 1, 0, 0, 1)
    put("sources", "Q", 1)
    put("origin", "I", 0)
    put("kind", "I", 1)
    put("zoom", "i", 1)
    put("x", "I", 1)
    put("y", "I", 0)
    put("cell_present", "B", cell is not None)
    if cell is not None:
        put("cell", "ii", *cell)
    for name, raw in (("provider", provider), ("revision", b"r"), ("digest", b"d")):
        text(name, raw)
    put("feature_count", "I", 1)
    put("inputs", "Q", 1)
    put("source", "I", 0)
    put("feature", "I", 0)
    put("id_present", "B", identity is not None)
    if identity is not None:
        put("id", "Q", identity)
    for name, raw in (
        ("layer", b"water_polygons"),
        ("input_kind", b"river"),
        ("crossing", b""),
    ):
        text(name, raw)
    put("bridge", "B", 0)
    put("tunnel", "B", 0)
    put("intermittent_present", "B", intermittent is not None)
    if intermittent is not None:
        put("intermittent", "B", intermittent)
    return bytes(data), offsets


class WaterObservation(unittest.TestCase):
    def decode(self, data, expected=None, byte_limit=1 << 20):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.water-observation."
        ) as root:
            path = Path(root) / "input.native"
            path.write_bytes(data)
            return read_v3(
                path,
                expected or hashlib.sha256(data).hexdigest(),
                byte_limit=byte_limit,
            )

    def original_case(self):
        manifest = json.loads(
            (FIXTURES / "feldkirch-ill-native-inputs.json").read_text()
        )
        legacy = json.loads((FIXTURES / manifest["fixture"]).read_text())
        original = read_v2(
            FIXTURES / manifest["historical_v2"]["file"],
            manifest["historical_v2"]["sha256"],
        )
        observed = read_v3(
            FIXTURES / manifest["observed_v3"]["file"],
            manifest["observed_v3"]["sha256"],
        )
        return original, observed, manifest, legacy

    def test_all_original_bytes_and_selected_metadata_bindings(self):
        original, observed, manifest, legacy = self.original_case()
        self.assertEqual(observed["consumed_bytes"], 350449)
        self.assertEqual(
            (len(observed["sources"]), len(observed["inputs"])), (49, 1482)
        )
        for key in (
            "surfaces",
            "rings",
            "courses",
            "levels",
            "points_lat_lon",
            "tiles",
        ):
            self.assertFalse(observed[key].flags.writeable)
            with self.assertRaises(ValueError):
                observed[key].setflags(write=True)
        result = compare(original, observed, manifest, legacy)
        self.assertEqual(
            [len(result["geometry_mapping"][k]) for k in ("surfaces", "courses")],
            [550, 935],
        )
        self.assertEqual(
            [
                len(result["selected_observation_bindings"][k])
                for k in ("surfaces", "courses")
            ],
            [13, 17],
        )
        self.assertTrue(
            all(
                r["unique_metadata"]
                for rows in result["selected_observation_bindings"].values()
                for r in rows
            )
        )

    def test_optional_values_arbitrary_native_text_bytes_and_full_uint64(self):
        for identity in (None, 0, (1 << 53) + 1, (1 << 64) - 1):
            for intermittent in (None, False, True):
                raw, _ = encoded_control(
                    identity, intermittent, b"p\xff\x00", (-90, -180)
                )
                decoded = self.decode(raw)
                self.assertEqual(
                    decoded["inputs"][0]["provider_feature_id"],
                    None if identity is None else str(identity),
                )
                self.assertIs(decoded["inputs"][0]["intermittent"], intermittent)
                self.assertEqual(
                    decoded["sources"][0]["provider"].encode(
                        "utf-8", "surrogateescape"
                    ),
                    b"p\xff\x00",
                )
                self.assertEqual(decoded["sources"][0]["cell"], [-90, -180])

    def test_bad_boolean_count_text_source_and_geometry_references(self):
        raw, offsets = encoded_control(cell=(0, 0))
        controls = [
            (name, "B", 2)
            for name in (
                "cell_present",
                "id_present",
                "bridge",
                "tunnel",
                "intermittent_present",
            )
        ]
        controls += [
            ("provider_size", "I", (1 << 32) - 1),
            ("inputs", "Q", (1 << 64) - 1),
            ("sources", "Q", (1 << 64) - 1),
            ("source", "I", 1),
            ("feature", "I", 1),
            ("surface_input", "I", 1),
            ("course_input", "I", 1),
            ("origin", "I", 4),
            ("kind", "I", 5),
            ("zoom", "i", 31),
            ("x", "I", 2),
            ("cell", "i", 90),
        ]
        for name, code, value in controls:
            with self.subTest(name=name):
                changed = bytearray(raw)
                struct.pack_into("<" + code, changed, offsets[name], value)
                with self.assertRaises(ValueError):
                    self.decode(changed)
        changed = bytearray(raw)
        for name in ("surface_input", "course_input"):
            struct.pack_into("<I", changed, offsets[name], NO_INPUT)
        self.assertEqual(self.decode(changed)["surfaces"][0]["input"], NO_INPUT)

    def test_missing_trailing_foreign_version_digest_and_byte_budget(self):
        raw, _offsets = encoded_control()
        for changed in (raw[:20], raw[:-1], raw + b"x", b"WATER2\0\0" + raw[8:]):
            with self.assertRaises(ValueError):
                self.decode(changed)
        with self.assertRaisesRegex(ValueError, "digest"):
            self.decode(raw, "0" * 64)
        with self.assertRaisesRegex(ValueError, "budget"):
            self.decode(raw, byte_limit=len(raw) - 1)

    def test_existing_observation_cannot_hide_changed_geometry_identity_or_selection(
        self,
    ):
        original, observed, manifest, legacy = self.original_case()
        changed = dict(observed, levels=observed["levels"].copy())
        changed["levels"][0] += 1
        with self.assertRaisesRegex(ValueError, "permutation"):
            compare(original, changed, manifest, legacy)
        for field in ("kind", "provider_feature_id", "source"):
            changed = copy.deepcopy(legacy)
            changed["source_semantics_observation"]["surfaces"][0][field] = "changed"
            with self.assertRaisesRegex(ValueError, "binding"):
                compare(original, observed, manifest, changed)
        changed = copy.deepcopy(legacy)
        changed["source_semantics_observation"]["sources"]["2"]["revision"] = "changed"
        with self.assertRaisesRegex(ValueError, "identity"):
            compare(original, observed, manifest, changed)
        changed = copy.deepcopy(legacy)
        changed["source_semantics_observation"]["courses"][1] = changed[
            "source_semantics_observation"
        ]["courses"][0]
        with self.assertRaisesRegex(ValueError, "selection"):
            compare(original, observed, manifest, changed)

    def test_ambiguous_course_geometry_retains_all_metadata_candidates(self):
        original, observed, manifest, legacy = self.original_case()
        baseline = compare(original, observed, manifest, legacy)
        candidate = baseline["geometry_mapping"]["courses"][0]["observed_indices"][0]
        old = dict(
            original,
            courses=np.concatenate([original["courses"], original["courses"][:1]]),
        )
        rows = np.concatenate(
            [observed["courses"], observed["courses"][candidate : candidate + 1]]
        )
        copied_input = dict(
            observed["inputs"][int(rows[-1]["input"])], provider_feature_id="1"
        )
        rows[-1]["input"] = len(observed["inputs"])
        new = dict(observed, courses=rows, inputs=observed["inputs"] + (copied_input,))
        result = compare(old, new, manifest, legacy)
        selected = result["selected_observation_bindings"]["courses"][0]
        self.assertEqual(len(selected["observed_indices"]), 2)
        self.assertEqual(len(selected["candidate_inputs"]), 2)
        self.assertFalse(selected["unique_metadata"])


if __name__ == "__main__":
    unittest.main()
