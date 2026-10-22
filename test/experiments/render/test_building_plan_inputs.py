"""Original index8 and synthetic index9 wire controls; no geometry or pin writer."""

import struct
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import numpy as np
import building_plan_inputs as wire

ARCHIVE = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-capture-movement/original-fine-cell.zip"
)


def original():
    with zipfile.ZipFile(ARCHIVE) as archive:
        return archive.read("subset.index")


def with_ceilings():
    data = original()
    fields = []
    prepared = wire.prepared

    def observe(reader, terrain_fields=False):
        record = prepared(reader, terrain_fields)
        fields.append((reader.at - struct.calcsize("<4dQ4d"), record))
        return record

    with patch.object(wire, "prepared", observe):
        reference = wire.read_plans(data)
    output = bytearray(data[:4] + struct.pack("<I", 9))
    cursor = 8
    contacts = {}
    for index, (offset, record) in enumerate(fields):
        output.extend(data[cursor:offset])
        if index % 2 == 0 and record["minimum_height_m"] == 0.0:
            contacts[index] = (
                len(output),
                record["local_first"],
                record["seat_asl_m"] + 1.25,
            )
            output.extend(
                struct.pack("<BId", 1, contacts[index][1], contacts[index][2])
            )
        else:
            output.append(0)
        cursor = offset
    output.extend(data[cursor:])
    return reference, output, contacts


class BuildingPlanInputTests(unittest.TestCase):
    def test_original_eight_retains_unknown_contacts(self):
        plans = wire.read_plans(original())
        self.assertEqual(len(plans["records"]), 32)
        self.assertTrue(all("terrain_ceiling" not in row for row in plans["records"]))

    def test_nine_carries_optional_contacts_without_changing_original_fields(self):
        reference, encoded, contacts = with_ceilings()
        self.assertTrue(contacts)
        actual = wire.read_plans(encoded)
        for name in ("points_latlon", "holes", "corner_asl_m"):
            np.testing.assert_array_equal(actual[name], reference[name])
        for index, (before, after) in enumerate(
            zip(reference["records"], actual["records"], strict=True)
        ):
            ceiling = after.pop("terrain_ceiling")
            self.assertEqual(before, after)
            if index in contacts:
                _, origin, height = contacts[index]
                self.assertEqual(
                    ceiling, {"origin_point": origin, "height_asl_m": height}
                )
            else:
                self.assertIsNone(ceiling)

    def test_wrong_source_frame_and_nonfinite_height_fail(self):
        _, encoded, contacts = with_ceilings()
        offset, origin, _ = next(iter(contacts.values()))
        for field_offset, value in (
            (1, struct.pack("<I", origin + 1)),
            (5, struct.pack("<d", float("nan"))),
        ):
            altered = bytearray(encoded)
            altered[offset + field_offset : offset + field_offset + len(value)] = value
            with self.assertRaises(ValueError):
                wire.read_plans(altered)

    def test_unknown_schema_is_not_guessed(self):
        encoded = bytearray(original())
        struct.pack_into("<I", encoded, 4, 10)
        with self.assertRaises(ValueError):
            wire.read_plans(encoded)


if __name__ == "__main__":
    unittest.main()
