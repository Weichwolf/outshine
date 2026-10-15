"""Offline semantic and counterexample checks for the forecast reference (#253)."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

from open_meteo import MAX_BYTES, normalize


def fixture():
    return {
        "latitude": 47.26,
        "longitude": 10.12,
        "elevation": 1772.0,
        "utc_offset_seconds": 7200,
        "hourly_units": {"time": "unixtime"},
        "hourly": {"time": [2200000000, 2200003600]},
    }


def encoded(data):
    raw = json.dumps(data).encode()
    receipt = {
        "http_status": 200,
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "acquired_utc": "2026-10-15T12:00:00Z",
        "url": "https://api.open-meteo.com/v1/forecast?latitude=47.256121&longitude=10.115857"
        "&elevation=1772&timeformat=unixtime&timezone=GMT",
    }
    return raw, receipt


def sample(data):
    return normalize(*encoded(data))["samples"][0]


class Controls(unittest.TestCase):
    def test_captured_forecast_preserves_high_clouds_without_inventing_a_low_deck(self):
        root = Path(__file__).parent / "data" / "open-meteo"
        raw = (root / "koerbersee-20261015.json").read_bytes()
        receipt = json.loads((root / "koerbersee-20261015.receipt.json").read_bytes())
        result = normalize(raw, receipt)
        self.assertEqual(len(result["samples"]), 24)
        first = result["samples"][0]
        self.assertEqual(first["utc_seconds"], 1792022400)
        self.assertEqual(first["cloud_cover_fraction"], 0.3)
        self.assertEqual(first["cloud_low_fraction"], 0.0)
        self.assertEqual(first["cloud_high_fraction"], 0.3)
        self.assertIsNone(first["cloud_base_agl_m"])
        self.assertEqual(result["requested_position"]["latitude_deg"], 47.256121)
        self.assertEqual(result["model_grid_position"]["latitude_deg"], 47.26403)
        self.assertEqual(result["downscaling_surface_elevation_asl_m"], 1772.0)
        self.assertNotIn("surface_elevation_asl_m", result["model_grid_position"])

    def test_null_missing_and_known_zero_are_distinct(self):
        data = fixture()
        data["hourly"]["cloud_cover"] = [0, None]
        data["hourly_units"]["cloud_cover"] = "%"
        result = normalize(*encoded(data))["samples"]
        self.assertEqual(result[0]["cloud_cover_fraction"], 0.0)
        self.assertIsNone(result[1]["cloud_cover_fraction"])
        self.assertIsNone(result[0]["cloud_low_fraction"])
        self.assertIsNone(result[0]["cloud_base_agl_m"])
        self.assertIsNone(result[0]["vertical_wind_m_s"])

    def test_unknown_column_does_not_invent_units_or_zero(self):
        data = fixture()
        data["hourly"]["visibility"] = [None, None]
        self.assertIsNone(sample(data)["visibility_m"])

    def test_si_values_keep_their_sampling_heights(self):
        data = fixture()
        data["hourly"].update(
            temperature_2m=[-10, 0],
            relative_humidity_2m=[75, 0],
            surface_pressure=[1013.25, 832.2],
            cloud_cover_high=[25, 0],
        )
        data["hourly_units"].update(
            temperature_2m="°C",
            relative_humidity_2m="%",
            surface_pressure="hPa",
            cloud_cover_high="%",
        )
        result = sample(data)
        self.assertEqual(result["air_temperature_2m_k"], 263.15)
        self.assertEqual(result["relative_humidity_2m_fraction"], 0.75)
        self.assertEqual(result["surface_pressure_pa"], 101325.0)
        self.assertEqual(result["cloud_high_fraction"], 0.25)

    def test_unix_time_is_not_shifted_by_the_response_timezone(self):
        result = normalize(*encoded(fixture()))
        self.assertEqual(result["samples"][0]["utc_seconds"], 2200000000)
        self.assertEqual(result["source"]["utc_offset_seconds"], 7200)
        self.assertIsNone(result["source"]["model_run_utc"])
        self.assertEqual(result["source"]["kind"], "model_forecast")

    def test_precipitation_is_a_previous_hour_amount(self):
        data = fixture()
        data["hourly"]["precipitation"] = [3.6, 0.0]
        data["hourly_units"]["precipitation"] = "mm"
        result = sample(data)["precipitation_previous_hour"]
        self.assertEqual(result["begin_utc_seconds"], 2199996400)
        self.assertEqual(result["end_utc_seconds"], 2200000000)
        self.assertEqual(result["precipitation_water_kg_m2"], 3.6)
        self.assertIsNone(result["rain_water_kg_m2"])
        self.assertNotIn("rate_kg_m2_s", result)

    def test_snow_depth_does_not_claim_water_equivalent(self):
        data = fixture()
        data["hourly"].update(snowfall=[7.0, 0.0], snow_depth=[0.5, 0.5])
        data["hourly_units"].update(snowfall="cm", snow_depth="m")
        result = sample(data)
        self.assertEqual(
            result["precipitation_previous_hour"]["snowfall_depth_m"], 0.07
        )
        self.assertEqual(result["snow_depth_m"], 0.5)
        self.assertNotIn("snow_mass_kg_m2", result)

    def test_wind_from_cardinals_and_full_turn(self):
        for bearing, expected in [
            (0, [0.0, 8.0]),
            (90, [-8.0, 0.0]),
            (180, [0.0, -8.0]),
            (270, [8.0, 0.0]),
            (360, [0.0, 8.0]),
        ]:
            with self.subTest(bearing=bearing):
                data = fixture()
                data["hourly"].update(
                    wind_speed_10m=[8, 8], wind_direction_10m=[bearing, bearing]
                )
                data["hourly_units"].update(
                    wind_speed_10m="m/s", wind_direction_10m="°"
                )
                result = sample(data)
                for actual, wanted in zip(
                    result["wind_horizontal_10m_east_south_m_s"], expected
                ):
                    self.assertAlmostEqual(actual, wanted, delta=1e-12)
                self.assertEqual(result["wind_from_10m_deg"], bearing % 360)

    def test_calm_and_missing_direction_have_different_meaning(self):
        data = fixture()
        data["hourly"]["wind_speed_10m"] = [0, 2]
        data["hourly_units"]["wind_speed_10m"] = "m/s"
        result = normalize(*encoded(data))["samples"]
        self.assertEqual(result[0]["wind_horizontal_10m_east_south_m_s"], [0.0, 0.0])
        self.assertIsNone(result[0]["wind_from_10m_deg"])
        self.assertIsNone(result[1]["wind_horizontal_10m_east_south_m_s"])

    def test_query_and_grid_positions_remain_separate(self):
        data = fixture()
        data["elevation"] = None
        result = normalize(*encoded(data))
        self.assertEqual(result["requested_position"]["latitude_deg"], 47.256121)
        self.assertEqual(result["model_grid_position"]["latitude_deg"], 47.26)
        self.assertIsNone(result["downscaling_surface_elevation_asl_m"])
        self.assertEqual(
            result["requested_position"]["declared_surface_elevation_asl_m"], 1772
        )

    def test_malformed_values_are_rejected_instead_of_clamped(self):
        for values in ([101, 0], [-1, 0], [True, 0], [float("nan"), 0], [0]):
            with self.subTest(values=values):
                data = fixture()
                data["hourly"]["cloud_cover"] = values
                data["hourly_units"]["cloud_cover"] = "%"
                with self.assertRaises(ValueError):
                    sample(data)

    def test_units_are_not_guessed(self):
        data = fixture()
        data["hourly"]["wind_speed_10m"] = [36, 36]
        data["hourly_units"]["wind_speed_10m"] = "km/h"
        with self.assertRaises(ValueError):
            sample(data)

    def test_time_gaps_duplicates_and_nonintegers_are_rejected(self):
        for times in (
            [2200000000, 2200007200],
            [2200000000, 2200000000],
            [True, 2200003600],
            [2200000000.0, 2200003600],
            [],
        ):
            with self.subTest(times=times):
                data = fixture()
                data["hourly"]["time"] = times
                with self.assertRaises(ValueError):
                    sample(data)

    def test_receipt_must_match_the_actual_bytes(self):
        raw, receipt = encoded(fixture())
        with self.assertRaises(ValueError):
            normalize(raw + b" ", receipt)
        refused = copy.deepcopy(receipt)
        refused["http_status"] = 503
        with self.assertRaises(ValueError):
            normalize(raw, refused)

    def test_boundaries_and_acquisition_time_are_explicit(self):
        raw, receipt = encoded(fixture())
        with self.assertRaises(ValueError):
            normalize(b" " * (MAX_BYTES + 1), receipt)
        receipt["acquired_utc"] = "2026-10-15T12:00:00"
        with self.assertRaises(ValueError):
            normalize(raw, receipt)


if __name__ == "__main__":
    unittest.main()
