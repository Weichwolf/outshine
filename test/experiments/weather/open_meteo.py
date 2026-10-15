"""Offline reference for explicit hourly Open-Meteo forecast normalization (#253)."""

import argparse
import hashlib
import json
import math
from dataclasses import dataclass
from datetime import UTC, datetime
from itertools import pairwise
from pathlib import Path
from urllib.parse import parse_qs, urlsplit


@dataclass(frozen=True)
class Field:
    source: str
    target: str
    unit: str
    scale: float = 1.0
    offset: float = 0.0
    minimum: float = 0.0
    maximum: float = math.inf
    previous_hour: bool = False


FIELDS = (
    Field("cloud_cover", "cloud_cover_fraction", "%", 0.01, maximum=1.0),
    Field("cloud_cover_low", "cloud_low_fraction", "%", 0.01, maximum=1.0),
    Field("cloud_cover_mid", "cloud_mid_fraction", "%", 0.01, maximum=1.0),
    Field("cloud_cover_high", "cloud_high_fraction", "%", 0.01, maximum=1.0),
    Field("wind_speed_10m", "wind_speed_10m_m_s", "m/s"),
    Field("wind_direction_10m", "wind_from_10m_deg", "°", maximum=360.0),
    Field("temperature_2m", "air_temperature_2m_k", "°C", offset=273.15),
    Field(
        "relative_humidity_2m", "relative_humidity_2m_fraction", "%", 0.01, maximum=1.0
    ),
    Field("visibility", "visibility_m", "m"),
    Field("surface_pressure", "surface_pressure_pa", "hPa", 100.0),
    Field("snow_depth", "snow_depth_m", "m"),
    Field("precipitation", "precipitation_water_kg_m2", "mm", previous_hour=True),
    Field("rain", "rain_water_kg_m2", "mm", previous_hour=True),
    Field("showers", "showers_water_kg_m2", "mm", previous_hour=True),
    Field("snowfall", "snowfall_depth_m", "cm", 0.01, previous_hour=True),
)
MAX_BYTES = 1024 * 1024
MAX_SAMPLES = 4096
HOUR_S = 3600


def number(value, label, minimum=-math.inf, maximum=math.inf):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError(f"{label}: finite number required")
    if not minimum <= value <= maximum:
        raise ValueError(f"{label}: value outside [{minimum}, {maximum}]")
    return float(value)


def utc_seconds(value):
    if type(value) is not int or not -(2**63) + HOUR_S <= value < 2**63:
        raise ValueError("hourly.time: signed integer UTC seconds required")
    return value


def acquired_time(text):
    if not isinstance(text, str):
        raise TypeError("receipt: acquisition UTC required")
    acquired = datetime.fromisoformat(text)
    if acquired.tzinfo is None:
        raise ValueError("receipt: acquisition time requires an explicit offset")
    return acquired.astimezone(UTC).isoformat()


def request_details(uri):
    if not isinstance(uri, str):
        raise TypeError("receipt: source URL required")
    parsed = urlsplit(uri)
    if (
        parsed.scheme != "https"
        or parsed.hostname != "api.open-meteo.com"
        or parsed.path != "/v1/forecast"
    ):
        raise ValueError("receipt: expected the Open-Meteo HTTPS forecast endpoint")
    query = parse_qs(parsed.query)

    def scalar(name, minimum=-math.inf, maximum=math.inf, optional=False):
        values = query.get(name)
        if values is None and optional:
            return None
        if not values or len(values) != 1:
            raise ValueError(f"receipt: one {name} required")
        return number(float(values[0]), name, minimum, maximum)

    if query.get("timeformat") != ["unixtime"]:
        raise ValueError("receipt: Unix UTC timestamps must be requested explicitly")
    position = {
        "latitude_deg": scalar("latitude", -90.0, 90.0),
        "longitude_deg": scalar("longitude", -180.0, 180.0),
        "declared_surface_elevation_asl_m": scalar("elevation", optional=True),
    }
    return position, query.get("models")


def columns(hourly, units, count):
    result = {}
    for field in FIELDS:
        if field.source not in hourly:
            result[field.source] = [None] * count
            continue
        values = hourly[field.source]
        if not isinstance(values, list) or len(values) != count:
            raise ValueError(f"{field.source}: array length must equal hourly.time")
        if (
            any(value is not None for value in values)
            and units.get(field.source) != field.unit
        ):
            raise ValueError(f"{field.source}: explicit unit {field.unit!r} required")
        converted = []
        for value in values:
            if value is None:
                converted.append(None)
                continue
            value = number(value, field.source) * field.scale + field.offset
            converted.append(number(value, field.target, field.minimum, field.maximum))
        result[field.source] = converted
    return result


def horizontal_wind(speed, bearing):
    if speed is None:
        return None
    if speed == 0.0:
        return [0.0, 0.0]
    if bearing is None:
        return None
    angle = math.radians(bearing)
    return [-speed * math.sin(angle), speed * math.cos(angle)]


def sample_at(at, index, values):
    sample = {"utc_seconds": at, "cloud_base_agl_m": None, "vertical_wind_m_s": None}
    precipitation = {"begin_utc_seconds": at - HOUR_S, "end_utc_seconds": at}
    for field in FIELDS:
        value = values[field.source][index]
        if field.target == "wind_from_10m_deg" and value is not None:
            value %= 360.0
        target = precipitation if field.previous_hour else sample
        target[field.target] = value
    sample["wind_horizontal_10m_east_south_m_s"] = horizontal_wind(
        sample["wind_speed_10m_m_s"], sample["wind_from_10m_deg"]
    )
    sample["precipitation_previous_hour"] = precipitation
    return sample


def normalize(raw, receipt):
    if len(raw) > MAX_BYTES:
        raise ValueError("forecast response exceeds the reference input limit")
    if receipt.get("http_status") != 200:
        raise ValueError("receipt: successful HTTP response required")
    digest = hashlib.sha256(raw).hexdigest()
    if receipt.get("source_sha256") != digest:
        raise ValueError("receipt: source byte hash mismatch")
    data = json.loads(raw.decode("utf-8"), parse_constant=reject_constant)
    if not isinstance(data, dict) or data.get("error"):
        raise ValueError("forecast response must be one successful location object")
    hourly, units = data.get("hourly"), data.get("hourly_units")
    if (
        not isinstance(hourly, dict)
        or not isinstance(units, dict)
        or units.get("time") != "unixtime"
    ):
        raise ValueError("forecast requires hourly arrays and explicit Unix-time units")
    times = hourly.get("time")
    if not isinstance(times, list) or not 1 <= len(times) <= MAX_SAMPLES:
        raise ValueError("hourly.time: bounded nonempty array required")
    times = [utc_seconds(value) for value in times]
    if any(later - earlier != HOUR_S for earlier, later in pairwise(times)):
        raise ValueError("forecast requires consecutive hourly UTC samples")
    values = columns(hourly, units, len(times))
    position, models = request_details(receipt.get("url"))
    offset = data.get("utc_offset_seconds")
    if offset is not None and type(offset) is not int:
        raise ValueError("utc_offset_seconds: integer or unknown required")
    return {
        "schema": "outshine.open-meteo.hourly-reference.v1",
        "source": {
            "provider": "open-meteo",
            "kind": "model_forecast",
            "uri": receipt.get("url"),
            "source_sha256": digest,
            "acquired_utc": acquired_time(receipt.get("acquired_utc")),
            "model_run_utc": None,
            "model_selection": "explicit_request" if models else "provider_automatic",
            "requested_models": models,
            "utc_offset_seconds": offset,
        },
        "requested_position": position,
        "model_grid_position": {
            "latitude_deg": optional_number(
                data.get("latitude"), "grid latitude", -90.0, 90.0
            ),
            "longitude_deg": optional_number(
                data.get("longitude"), "grid longitude", -180.0, 180.0
            ),
        },
        "downscaling_surface_elevation_asl_m": optional_number(
            data.get("elevation"), "downscaling surface elevation"
        ),
        "samples": [sample_at(at, index, values) for index, at in enumerate(times)],
    }


def reject_constant(text):
    raise ValueError(f"nonstandard JSON numeric constant: {text}")


def optional_number(value, label, minimum=-math.inf, maximum=math.inf):
    return None if value is None else number(value, label, minimum, maximum)


def read_bounded(path):
    with path.open("rb") as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError(f"input exceeds the reference byte limit: {path}")
    return raw


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = normalize(read_bounded(args.input), json.loads(read_bounded(args.receipt)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(
        f"normalized {len(result['samples'])} hourly model samples; no runtime/Place claim"
    )


if __name__ == "__main__":
    main()
