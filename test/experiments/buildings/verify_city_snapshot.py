"""Pinned city replay cannot silently follow a changed provider endpoint."""

import unittest
from unittest.mock import patch

from city_survey_sources import read

TILE = (14, 8298, 5635)
RECEIPT = {
    "kind": "vector",
    "tile": list(TILE),
    "url": "https://tiles.openfreemap.org/planet/pinned/14/8298/5635.pbf",
    "sha256": "a" * 64,
}


class CitySnapshotTests(unittest.TestCase):
    def test_explicit_snapshot_does_not_consult_or_follow_the_current_provider(self):
        with (
            patch("city_survey_sources.covering_tiles", return_value=(TILE,)),
            patch(
                "city_survey_sources.endpoint",
                side_effect=AssertionError("current provider consulted"),
            ),
            patch("city_survey_sources.acquire", return_value=RECEIPT) as acquire,
            patch("city_survey_sources.decode_vector", return_value={}),
        ):
            buildings, pois, source = read("Paris", False, snapshot=(RECEIPT,))
        self.assertEqual(buildings, [])
        self.assertEqual(pois, [])
        self.assertEqual(acquire.call_args.args[0], ("vector", TILE, RECEIPT["url"]))
        self.assertFalse(acquire.call_args.kwargs["fetch"])
        self.assertEqual(source["receipts"], [RECEIPT])
        self.assertEqual(
            source["provider_definition"], "explicit pinned source receipts"
        )
        self.assertIsNone(source["provider_sha256"])

    def test_incomplete_duplicate_or_invalid_snapshot_rejects_before_acquisition(self):
        invalid = (
            (),
            (RECEIPT, RECEIPT),
            (RECEIPT | {"tile": [14, 8298, 5636]},),
            (RECEIPT | {"sha256": "invalid"},),
            (RECEIPT | {"kind": "elevation"},),
        )
        for snapshot in invalid:
            with (
                self.subTest(snapshot=snapshot),
                patch("city_survey_sources.covering_tiles", return_value=(TILE,)),
                patch(
                    "city_survey_sources.acquire",
                    side_effect=AssertionError("invalid scope acquired"),
                ),
                self.assertRaises(ValueError),
            ):
                read("Paris", False, snapshot=snapshot)

    def test_changed_payload_is_rejected_before_any_geometry_decode(self):
        with (
            patch("city_survey_sources.covering_tiles", return_value=(TILE,)),
            patch(
                "city_survey_sources.acquire",
                return_value=RECEIPT | {"sha256": "b" * 64},
            ),
            patch(
                "city_survey_sources.decode_vector",
                side_effect=AssertionError("changed source decoded"),
            ),
            self.assertRaisesRegex(ValueError, "hash changed before decode"),
        ):
            read("Paris", False, snapshot=(RECEIPT,))

    def test_unpinned_survey_keeps_its_explicit_current_provider_behavior(self):
        with (
            patch("city_survey_sources.covering_tiles", return_value=(TILE,)),
            patch(
                "city_survey_sources.endpoint",
                return_value=(
                    "https://provider/current/{z}/{x}/{y}.pbf",
                    "provider.py",
                    "current-hash",
                ),
            ),
            patch("city_survey_sources.acquire", return_value=RECEIPT) as acquire,
            patch("city_survey_sources.decode_vector", return_value={}),
        ):
            _, _, source = read("Paris", False)
        self.assertEqual(
            acquire.call_args.args[0][2], "https://provider/current/14/8298/5635.pbf"
        )
        self.assertEqual(source["provider_definition"], "provider.py")
        self.assertEqual(source["provider_sha256"], "current-hash")


if __name__ == "__main__":
    unittest.main()
