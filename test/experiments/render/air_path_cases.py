"""Place camera inputs and the current native EarthSunPos input approximation."""

import math
import xml.etree.ElementTree as ET
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path

import numpy as np


@dataclass(frozen=True)
class RayCase:
    name: str
    height_km: float
    elevation_deg: float
    sun_elevation_deg: float
    relative_azimuth_deg: float
    utc: str

    def arguments(self):
        return (
            self.height_km,
            math.sin(math.radians(self.elevation_deg)),
            math.sin(math.radians(self.sun_elevation_deg)),
            math.cos(math.radians(self.relative_azimuth_deg)),
        )

    def record(self):
        return asdict(self)


def native_sun_formula(latitude_deg, longitude_deg, utc):
    """Keep the experiment's solar inputs consistent with Ephemeris.h::EarthSunPos."""
    unix_s = datetime.fromisoformat(utc).timestamp()
    days = unix_s / 86400.0 + 2440587.5 - 2451545.0
    rad = math.pi / 180
    longitude = math.fmod(280.460 + 0.9856474 * days, 360)
    anomaly = math.fmod(357.528 + 0.9856003 * days, 360) * rad
    ecliptic = (
        longitude + 1.915 * math.sin(anomaly) + 0.020 * math.sin(2 * anomaly)
    ) * rad
    obliquity = (23.439 - 0.0000004 * days) * rad
    declination = math.asin(math.sin(obliquity) * math.sin(ecliptic))
    ascension = math.atan2(math.cos(obliquity) * math.sin(ecliptic), math.cos(ecliptic))
    gmst = math.fmod(18.697374558 + 24.06570982441908 * days, 24)
    sidereal = math.fmod(gmst * 15 + longitude_deg, 360)
    hour = (sidereal - ascension / rad) * rad
    latitude = latitude_deg * rad
    elevation = (
        math.asin(
            math.sin(latitude) * math.sin(declination)
            + math.cos(latitude) * math.cos(declination) * math.cos(hour)
        )
        / rad
    )
    azimuth = math.fmod(
        math.atan2(
            -math.sin(hour),
            math.tan(declination) * math.cos(latitude)
            - math.sin(latitude) * math.cos(hour),
        )
        / rad
        + 360,
        360,
    )
    return elevation, azimuth


def place_cases(root: Path):
    cases, inputs = [], []
    for name in ("Rosenheim", "Koerbersee", "Malcesine"):
        path = root / "src/assets/places" / (name + ".scenario")
        inputs.append(path)
        tree = ET.parse(path).getroot()
        world, camera, clock = (
            tree.find("world"),
            tree.find("views/view/at"),
            tree.find("clock"),
        )
        if float(world.get("haze", "0")) != 0:
            raise ValueError(
                "this comparison isolates the existing clear-gas declarations"
            )
        height = float(camera.get("heightM")) / 1000.0
        pitch = float(camera.get("pitchDeg"))
        bearing = float(camera.get("bearingDeg"))
        latitude, longitude = float(world.get("lat")), float(world.get("lon"))
        for utc in [clock.get("start"), "2026-09-07T04:00:00Z", "2026-09-07T18:00:00Z"]:
            sun_elevation, sun_azimuth = native_sun_formula(latitude, longitude, utc)
            for elevation in [pitch, 30.0]:
                label = f"{name}/{utc}/elevation={elevation:g}"
                cases.append(
                    RayCase(
                        label,
                        height,
                        elevation,
                        sun_elevation,
                        sun_azimuth - bearing,
                        utc,
                    )
                )
    return cases, inputs


def usable_distances(medium, case):
    radius = medium.bottom_km + case.height_km
    cosine = case.arguments()[1]
    to_top = -radius * cosine + math.sqrt(
        radius**2 * (cosine**2 - 1) + medium.top_km**2
    )
    discriminant = radius**2 * (cosine**2 - 1) + medium.bottom_km**2
    to_ground = (
        -radius * cosine - math.sqrt(discriminant) if discriminant >= 0 else math.inf
    )
    boundary = min(to_top, to_ground if to_ground >= 0 else math.inf)
    return np.array(
        [d for d in [0.001, 0.01, 0.05, 0.5, 5.0, 20.0, 100.0, 200.0] if d < boundary]
    )
