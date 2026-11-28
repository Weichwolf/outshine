"""Native scenario cameras in one explicit WGS84 east/north/up frame."""

import hashlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import numpy as np


def ecef(latitude, longitude, height):
    latitude, longitude, height = np.broadcast_arrays(latitude, longitude, height)
    if (
        not all(np.isfinite(value).all() for value in (latitude, longitude, height))
        or np.any(abs(latitude) > 90)
        or np.any(abs(longitude) > 180)
    ):
        raise ValueError("finite WGS84 geographic coordinates required")
    lat, lon = np.deg2rad(latitude), np.deg2rad(longitude)
    eccentricity2 = 6.6943799901413165e-3
    prime = 6378137.0 / np.sqrt(1 - eccentricity2 * np.sin(lat) ** 2)
    return np.stack(
        (
            (prime + height) * np.cos(lat) * np.cos(lon),
            (prime + height) * np.cos(lat) * np.sin(lon),
            (prime * (1 - eccentricity2) + height) * np.sin(lat),
        ),
        axis=-1,
    )


def axes(latitude, longitude):
    lat, lon = np.deg2rad((latitude, longitude))
    return np.array(
        (
            (-np.sin(lon), np.cos(lon), 0),
            (-np.sin(lat) * np.cos(lon), -np.sin(lat) * np.sin(lon), np.cos(lat)),
            (np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)),
        )
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
    sight_m: float

    def __post_init__(self):
        values = (
            self.latitude_deg,
            self.longitude_deg,
            self.origin_latitude_deg,
            self.origin_longitude_deg,
            self.declared_height_m,
            self.bearing_deg,
            self.pitch_deg,
            self.vertical_fov_deg,
            self.sight_m,
        )
        if (
            not np.isfinite(values).all()
            or max(abs(self.latitude_deg), abs(self.origin_latitude_deg)) > 90
            or max(abs(self.longitude_deg), abs(self.origin_longitude_deg)) > 180
            or abs(self.pitch_deg) > 90
            or not 0 < self.vertical_fov_deg < 180
            or self.sight_m <= 0
            or type(self.samples_height) is not bool
            or not isinstance(self.native_profile, tuple)
            or len(self.native_profile) != 2
            or any(
                type(value) is not int or value <= 0 for value in self.native_profile
            )
        ):
            raise ValueError("finite geographic pose, native frame and lens required")
        if datetime.fromisoformat(self.time_utc).utcoffset() is None:
            raise ValueError("explicit timezone required for the fixed clock")

    def local(self, latitude, longitude, height):
        basis = axes(self.origin_latitude_deg, self.origin_longitude_deg)
        anchor = ecef(self.origin_latitude_deg, self.origin_longitude_deg, 0)
        return (ecef(latitude, longitude, height) - anchor) @ basis.T

    def eye(self, ground_height_m=None):
        height = self.declared_height_m
        if self.samples_height:
            ground = np.asarray(ground_height_m)
            if (
                ground.shape != ()
                or ground.dtype.kind not in "fiu"
                or not np.isfinite(ground)
            ):
                raise ValueError("actual finite scalar DEM height required")
            height += float(ground)
        return self.local(self.latitude_deg, self.longitude_deg, height)

    @property
    def basis(self):
        bearing, pitch = np.deg2rad((self.bearing_deg, self.pitch_deg))
        station = axes(self.latitude_deg, self.longitude_deg)
        horizontal = station[0] * np.sin(bearing) + station[1] * np.cos(bearing)
        forward = horizontal * np.cos(pitch) + station[2] * np.sin(pitch)
        up = station[2] * np.cos(pitch) - horizontal * np.sin(pitch)
        rotation = axes(self.origin_latitude_deg, self.origin_longitude_deg)
        forward, up = rotation @ forward, rotation @ up
        return np.column_stack((np.cross(forward, up), up, -forward))

    def project(self, points, ground_height_m=None, profile=None):
        profile = self.native_profile if profile is None else profile
        if len(profile) != 2 or any(type(n) is not int or n <= 0 for n in profile):
            raise ValueError("positive pixel dimensions required")
        if (
            abs(
                profile[0] / profile[1]
                - self.native_profile[0] / self.native_profile[1]
            )
            > 1e-12
        ):
            raise ValueError("reference projection retains the native aspect ratio")
        points = np.asarray(points, dtype=float)
        if points.shape[-1:] != (3,) or not np.isfinite(points).all():
            raise ValueError("finite ENU triples required")
        camera = (points - self.eye(ground_height_m)) @ self.basis
        depth = -camera[..., 2]
        extent = np.array((profile[0] / profile[1], 1)) * np.tan(
            np.deg2rad(self.vertical_fov_deg) / 2
        )
        ndc = np.full(camera[..., :2].shape, np.nan)
        try:
            with np.errstate(over="raise", divide="raise", invalid="raise"):
                np.divide(
                    camera[..., :2],
                    depth[..., None] * extent,
                    out=ndc,
                    where=depth[..., None] > 0,
                )
                pixels = (ndc * (1, -1) + 1) * np.array(profile) / 2
        except FloatingPointError as error:
            raise ValueError("projected pixels are not representable") from error
        return pixels, depth


def read_camera(path):
    path = Path(path)
    raw = path.read_bytes()
    if not 0 < len(raw) <= 1024 * 1024:
        raise ValueError("scenario must contain one byte to one MiB")
    document = ET.fromstring(raw)
    world, render, clock = (document.find(key) for key in ("world", "render", "clock"))
    views = document.findall("views/view")
    if (
        world is None
        or render is None
        or clock is None
        or len(views) != 1
        or clock.get("live") != "no"
    ):
        raise ValueError("one native geographic Place camera and fixed clock required")
    view, at = views[0], views[0].find("at")
    if at is None or at.get("samplesHeight") not in ("yes", "no"):
        raise ValueError("explicit camera height reference required")
    return PlaceCamera(
        path.stem,
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
        hashlib.sha256(raw).hexdigest(),
        float(world.get("sightM")),
    )
