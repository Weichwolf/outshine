"""Cross-check the native decoder against the existing independent Python reference."""

import argparse
import copy
import hashlib
import json
import math
import subprocess
import time
from datetime import datetime
from pathlib import Path

from open_meteo import acquired_time, normalize, request_details

HERE = Path(__file__).resolve().parent


def row(sample):
    wind = sample["wind_horizontal_10m_east_south_m_s"]
    amount = sample["precipitation_previous_hour"]
    return [
        sample["utc_seconds"],
        *[
            sample[key]
            for key in (
                "cloud_cover_fraction",
                "cloud_low_fraction",
                "cloud_mid_fraction",
                "cloud_high_fraction",
                "wind_speed_10m_m_s",
                "wind_from_10m_deg",
            )
        ],
        *(wind if wind is not None else [None, None]),
        *[
            sample[key]
            for key in (
                "air_temperature_2m_k",
                "relative_humidity_2m_fraction",
                "visibility_m",
                "surface_pressure_pa",
                "snow_depth_m",
                "cloud_base_agl_m",
                "vertical_wind_m_s",
            )
        ],
        *[
            amount[key]
            for key in (
                "begin_utc_seconds",
                "end_utc_seconds",
                "precipitation_water_kg_m2",
                "rain_water_kg_m2",
                "showers_water_kg_m2",
                "snowfall_depth_m",
            )
        ],
    ]


def context(receipt):
    position, _ = request_details(receipt["url"])
    return {
        "url": receipt["url"],
        "http_status": receipt["http_status"],
        "source_sha256": receipt["source_sha256"],
        "latitude_deg": position["latitude_deg"],
        "longitude_deg": position["longitude_deg"],
        "elevation_asl_m": position["declared_surface_elevation_asl_m"],
        "acquired_utc_s": int(
            datetime.fromisoformat(acquired_time(receipt["acquired_utc"])).timestamp()
        ),
    }


def expected(value):
    requested, grid = value["requested_position"], value["model_grid_position"]
    return {
        "source_sha256": value["source"]["source_sha256"],
        "request": [
            requested["latitude_deg"],
            requested["longitude_deg"],
            requested["declared_surface_elevation_asl_m"],
        ],
        "grid": [
            grid["latitude_deg"],
            grid["longitude_deg"],
            value["downscaling_surface_elevation_asl_m"],
        ],
        "utc_offset": value["source"]["utc_offset_seconds"],
        "acquired_utc": int(
            datetime.fromisoformat(value["source"]["acquired_utc"]).timestamp()
        ),
        "model_run_utc": None,
        "hours": [row(sample) for sample in value["samples"]],
    }


def equal(actual, reference):
    if type(reference) is dict:
        return actual.keys() == reference.keys() and all(
            equal(actual[k], v) for k, v in reference.items()
        )
    if type(reference) is list:
        return (
            type(actual) is list
            and len(actual) == len(reference)
            and all(equal(a, b) for a, b in zip(actual, reference, strict=True))
        )
    if reference is None:
        return actual is None
    if type(reference) is int:
        return type(actual) is int and actual == reference
    if type(reference) is float:
        return isinstance(actual, (int, float)) and math.isclose(
            actual, reference, rel_tol=1e-13, abs_tol=1e-12
        )
    return actual == reference


def cases(original):
    result = [("captured", original)]

    def changed(name, mutate):
        value = json.loads(original)
        mutate(value)
        result.append((name, json.dumps(value, allow_nan=False).encode()))

    changed(
        "post-2038",
        lambda v: v["hourly"].update(time=[2208988800 + 3600 * i for i in range(24)]),
    )
    changed(
        "beyond-double-integer",
        lambda v: v["hourly"].update(time=[2**53 + 3600 * i for i in range(24)]),
    )
    changed(
        "int64-upper",
        lambda v: v["hourly"].update(
            time=[2**63 - 1 - 3600 * (23 - i) for i in range(24)]
        ),
    )
    changed(
        "int64-lower",
        lambda v: v["hourly"].update(
            time=[-(2**63) + 3600 * (i + 1) for i in range(24)]
        ),
    )
    changed(
        "no-scalars",
        lambda v: v.update(
            hourly={"time": v["hourly"]["time"]}, hourly_units={"time": "unixtime"}
        ),
    )
    changed(
        "calm-without-bearing",
        lambda v: v["hourly"].update(
            wind_speed_10m=[0] * 24, wind_direction_10m=[None] * 24
        ),
    )
    changed(
        "nonzero-without-bearing",
        lambda v: v["hourly"].update(
            wind_speed_10m=[3] * 24, wind_direction_10m=[None] * 24
        ),
    )
    changed(
        "wind-cardinals",
        lambda v: v["hourly"].update(
            wind_speed_10m=[7] * 24, wind_direction_10m=[0, 90, 180, 270, 360, 45] * 4
        ),
    )
    changed(
        "unknown-unit-for-null",
        lambda v: (
            v["hourly"].update(surface_pressure=[None] * 24),
            v["hourly_units"].pop("surface_pressure"),
        ),
    )
    changed("zero-kelvin", lambda v: v["hourly"].update(temperature_2m=[-273.15] * 24))
    changed("wrong-units", lambda v: v["hourly_units"].update(wind_speed_10m="km/h"))
    changed("null-time", lambda v: v["hourly"]["time"].__setitem__(0, None))
    changed("boolean-time", lambda v: v["hourly"]["time"].__setitem__(0, True))
    changed(
        "floating-time",
        lambda v: v["hourly"]["time"].__setitem__(0, float(v["hourly"]["time"][0])),
    )
    changed(
        "nonconsecutive-time",
        lambda v: v["hourly"]["time"].__setitem__(1, v["hourly"]["time"][1] + 1),
    )
    changed("time-overflow", lambda v: v["hourly"].update(time=[2**63] * 24))
    changed(
        "precipitation-begin-overflow",
        lambda v: v["hourly"].update(time=[-(2**63)] * 24),
    )
    changed("empty-time", lambda v: v["hourly"].update(time=[]))
    changed(
        "maximum-hours",
        lambda v: v.update(hourly={"time": [3600 * (i + 1) for i in range(4096)]}),
    )
    changed("too-many-hours", lambda v: v["hourly"].update(time=list(range(4097))))
    changed("short-column", lambda v: v["hourly"].update(cloud_cover=[0]))
    changed("invalid-column", lambda v: v["hourly"].update(cloud_cover=None))
    changed("boolean-value", lambda v: v["hourly"]["cloud_cover"].__setitem__(0, False))
    changed("negative-rain", lambda v: v["hourly"]["rain"].__setitem__(0, -1))
    changed("too-much-cloud", lambda v: v["hourly"]["cloud_cover"].__setitem__(0, 101))
    changed(
        "below-zero-kelvin",
        lambda v: v["hourly"]["temperature_2m"].__setitem__(0, -274),
    )
    changed("bad-grid", lambda v: v.update(latitude=91))
    changed("bad-offset", lambda v: v.update(utc_offset_seconds=0.0))
    changed("provider-error", lambda v: v.update(error=True))
    result.extend(
        [
            ("trailing-json", original + b" null"),
            ("invalid-json", b"{bad"),
            ("too-many-bytes", b" " * (1024 * 1024 + 1)),
            ("invalid-utf8", original[:-1] + b',"note":"\xff"}'),
        ]
    )
    return result


def sampling(probe, source, metadata, reference, name):
    hours = reference["hours"]
    by_time = {hour[0]: hour for hour in hours}
    stamps = [hours[0][0], hours[len(hours) // 2][0], hours[-1][0]]
    stamps.extend(
        at
        for at in (hours[0][0] - 1, hours[0][0] + 1, hours[-1][0] + 1)
        if -(2**63) <= at < 2**63
    )
    reports = []
    for index, at in enumerate(stamps):
        command = ["nice", "-n", "10", str(probe), str(source), str(metadata), str(at)]
        start = time.perf_counter()
        run = subprocess.run(
            command, capture_output=True, text=True, timeout=10, check=False
        )
        value = reference | {"hours": [by_time[at]]} if at in by_time else None
        matches = (
            run.returncode == 3
            if value is None
            else run.returncode == 0 and equal(json.loads(run.stdout), value)
        )
        reports.append(
            {
                "case": f"{name}/sample-{index}",
                "utc_s": at,
                "command": command,
                "exit_code": run.returncode,
                "matches": matches,
                "process_wall_s": time.perf_counter() - start,
                "stderr": run.stderr,
            }
        )
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.probe = args.probe.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    raw = (HERE / "data/open-meteo/koerbersee-20261015.json").read_bytes()
    original_receipt = json.loads(
        (HERE / "data/open-meteo/koerbersee-20261015.receipt.json").read_text()
    )
    reports = []
    for name, payload in cases(raw):
        receipt = copy.deepcopy(original_receipt)
        receipt["source_sha256"] = hashlib.sha256(payload).hexdigest()
        try:
            reference = expected(normalize(payload, receipt))
        except (ValueError, TypeError, UnicodeDecodeError):
            reference = None
        source, metadata = (
            args.output / f"{name}.json",
            args.output / f"{name}-context.json",
        )
        source.write_bytes(payload)
        metadata.write_text(json.dumps(context(receipt)))
        command = ["nice", "-n", "10", str(args.probe), str(source), str(metadata)]
        start = time.perf_counter()
        run = subprocess.run(
            command, capture_output=True, text=True, timeout=10, check=False
        )
        matches = (
            run.returncode == 2
            if reference is None
            else run.returncode == 0 and equal(json.loads(run.stdout), reference)
        )
        reports.append(
            {
                "case": name,
                "command": command,
                "exit_code": run.returncode,
                "reference_accepts": reference is not None,
                "matches": matches,
                "process_wall_s": time.perf_counter() - start,
                "stderr": run.stderr,
            }
        )
        if not matches:
            (args.output / f"{name}-native.txt").write_text(run.stdout)
        elif reference is not None:
            reports.extend(sampling(args.probe, source, metadata, reference, name))
    path = args.output / "results.json"
    path.write_text(
        json.dumps(
            {
                "checks": reports,
                "scope": "Native/reference normalization; process wall includes startup, IO and formatting, not frame or decode-only time.",
            },
            indent=2,
        )
    )
    failures = [r["case"] for r in reports if not r["matches"]]
    print(
        json.dumps({"cases": len(reports), "failures": failures, "results": str(path)})
    )
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
