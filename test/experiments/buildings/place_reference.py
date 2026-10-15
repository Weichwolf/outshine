"""Fixed native Place cameras in Python ENU, preserving height and vertical FOV semantics."""

import hashlib
import math
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from demand import View

ROOT = Path(__file__).resolve().parents[3]
PLACES = ("DarmstadtWest", "Flensburg", "Rosenheim", "Tokyo")


def ecef(latitude, longitude, height):
    lat, lon = np.deg2rad([latitude, longitude])
    eccentricity2 = 6.6943799901413165e-3
    prime = 6378137.0 / np.sqrt(1 - eccentricity2 * np.sin(lat) ** 2)
    return np.array(
        [
            (prime + height) * np.cos(lat) * np.cos(lon),
            (prime + height) * np.cos(lat) * np.sin(lon),
            (prime * (1 - eccentricity2) + height) * np.sin(lat),
        ]
    )


def axes(latitude, longitude):
    lat, lon = np.deg2rad([latitude, longitude])
    return np.array(
        [
            [-math.sin(lon), math.cos(lon), 0.0],
            [
                -math.sin(lat) * math.cos(lon),
                -math.sin(lat) * math.sin(lon),
                math.cos(lat),
            ],
            [
                math.cos(lat) * math.cos(lon),
                math.cos(lat) * math.sin(lon),
                math.sin(lat),
            ],
        ]
    )


@dataclass(frozen=True, slots=True)
class PlaceCamera:
    name: str
    latitude_deg: float
    longitude_deg: float
    origin_latitude_deg: float
    origin_longitude_deg: float
    declared_height_m: float
    samples_height: bool
    bearing_deg: float
    pitch_deg: float
    vertical_fov_deg: float
    native_profile: tuple[int, int]
    time_utc: str
    scenario_sha256: str

    def local(self, latitude, longitude, height):
        basis = axes(self.origin_latitude_deg, self.origin_longitude_deg)
        anchor = ecef(self.origin_latitude_deg, self.origin_longitude_deg, 0.0)
        coordinates = ecef(latitude, longitude, height)
        return (coordinates.T - anchor) @ basis.T

    def view(self, ground_height_m=None, profile=(1920, 1080)):
        if self.samples_height and (
            ground_height_m is None or not math.isfinite(ground_height_m)
        ):
            raise ValueError(
                "Native samplesHeight camera requires actual finite DEM height"
            )
        if (
            abs(
                profile[0] / profile[1]
                - self.native_profile[0] / self.native_profile[1]
            )
            > 1e-12
        ):
            raise ValueError("Place comparison requires the native aspect ratio")
        height = self.declared_height_m + (
            ground_height_m if self.samples_height else 0.0
        )
        eye = self.local(self.latitude_deg, self.longitude_deg, height)
        bearing, pitch = np.deg2rad([self.bearing_deg, self.pitch_deg])
        station = axes(self.latitude_deg, self.longitude_deg)
        horizontal = station[0] * math.sin(bearing) + station[1] * math.cos(bearing)
        forward = axes(self.origin_latitude_deg, self.origin_longitude_deg) @ (
            horizontal * math.cos(pitch) + station[2] * math.sin(pitch)
        )
        return View(
            eye,
            eye + forward,
            width=profile[0],
            height=profile[1],
            fov_deg=self.vertical_fov_deg,
        )


def read(name):
    if name not in PLACES:
        raise ValueError("Unknown fixed Buildings Place")
    path = ROOT / "src/assets/places" / (name + ".scenario")
    data = path.read_bytes()
    document = ET.fromstring(data)
    world, render, clock = (document.find(key) for key in ("world", "render", "clock"))
    views = document.findall("views/view")
    if len(views) != 1:
        raise ValueError("Place adapter requires one explicit station camera")
    view = views[0]
    at = view.find("at")
    if at.get("samplesHeight") not in ("yes", "no"):
        raise ValueError("Place camera height semantics must be explicit")
    return PlaceCamera(
        name,
        float(at.get("lat")),
        float(at.get("lon")),
        float(world.get("lat")),
        float(world.get("lon")),
        float(at.get("heightM")),
        at.get("samplesHeight") == "yes",
        float(at.get("bearingDeg")),
        float(at.get("pitchDeg")),
        float(view.get("fovDeg")),
        (int(render.get("widthPx")), int(render.get("heightPx"))),
        clock.get("start"),
        hashlib.sha256(data).hexdigest(),
    )
