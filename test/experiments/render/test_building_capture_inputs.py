"""Native inputs stay verifiable when producer success flags are wrong."""

import copy
import struct
import unittest
from pathlib import Path

from check_building_capture_inputs import check, load

FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-capture-movement/native-cell.json.gz"
)


class NativeCaptureInputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = load(FIXTURE)

    def test_native_controls_match(self):
        for path in (FIXTURE, FIXTURE.with_name("near-cell.json.gz")):
            with self.subTest(path=path.name):
                result = check(load(path))
                self.assertGreater(result[0]["sources"], 0)

    def test_one_float32_bit_in_the_native_control_is_detected(self):
        document = copy.deepcopy(self.original)
        data = document["results"][0]["projection_calls"][0]["capture"][
            "native_capture"
        ]["walls"]
        index = data["triangles"][0][0]
        word = struct.unpack("<I", struct.pack("<f", data["positions"][index][0]))[0]
        data["positions"][index][0] = struct.unpack("<f", struct.pack("<I", word ^ 1))[
            0
        ]
        with self.assertRaisesRegex(ValueError, "actual capture differs"):
            check(document)

    def test_invented_patch_ownership_is_detected(self):
        document = copy.deepcopy(self.original)
        capture = document["results"][0]["projection_calls"][0]["capture"]
        source = next(row for row in capture["sources"] if row["emission"] == 2)
        source["captured"][0]["part"] += 1
        with self.assertRaisesRegex(ValueError, "patch source changed"):
            check(document)

    def test_missing_full_fine_reference_is_detected(self):
        document = copy.deepcopy(self.original)
        del document["results"][0]["projection_calls"][0]["capture"]["sources"][0][
            "native_fine_reference"
        ]
        with self.assertRaisesRegex(
            ValueError, "complete native Fine reference missing"
        ):
            check(document)

    def test_an_empty_successful_projection_list_is_rejected(self):
        document = copy.deepcopy(self.original)
        document["results"][0]["projection_calls"] = []
        with self.assertRaisesRegex(ValueError, "no actual native projection"):
            check(document)

    def test_incorrect_triangle_accounting_is_detected(self):
        document = copy.deepcopy(self.original)
        document["results"][0]["projection_calls"][0]["capture"][
            "stored_triangles"
        ] -= 1
        with self.assertRaisesRegex(
            ValueError, "serialized triangle accounting differs"
        ):
            check(document)


if __name__ == "__main__":
    unittest.main()
